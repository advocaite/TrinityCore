/*
 * Playerbot V2 commands (.playerbot, .bot, .w/.tell, .learn all mylevel) and the module's chat listeners.
 * Registered by AddSC_playerbot_v2 (PlayerbotV2Scripts.cpp).
 */

#include "ScriptMgr.h"
#include "TypedName.h"
#include <set>
#include <unordered_set>

#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "Creature.h"
#include "GameObject.h"          // .playerbot bgzones â control-zone census
#include "Group.h"
#include "GroupMgr.h"
#include "Item.h"                // .bot itemstats
#include "Map.h"                 // .playerbot bgzones â object store + player list
#include "MapReference.h"
#include "GridDefines.h"         // INVALID_HEIGHT â .playerbot terrainprofile void check
#include "ObjectAccessor.h"
#include "ObjectMgr.h"            // .playerbot graveyardscan: world_safe_locs
#include "PhasingHandler.h"
#include "TerrainMgr.h"
#include "ClassicSpellValue.h"
#include "Log.h"
#include <fstream>
#include "Player.h"
#include "../Fleet/PlayerCraftOrders.h"   // .playerbot craftfor
#include "../Bot/BotBridge.h"           // .playerbot bridgeask
#include "GameTime.h"
#include "MovementPackets.h"
#include "WorldSession.h"
#include "TypeContainerVisitor.h"
#include "PlayerbotAPI.h"
#include "ClassicSpec.h"      // .playerbot questrun: spec override
#include "DB2Stores.h"        // .playerbot questrun: sMapStore
#include "PlayerbotV2.h"
#include "LfgBotFill.h"
#include "StringFormat.h"
#include "PlayerbotMovement.h"  // BotMovement::SafeTeleport for GM summonall
#include "RBAC.h"
#include "SpellHistory.h"
#include "World.h"
#include "Bot/Battleground/BgTeamCoordinator.h"
#include "Bot/Dungeon/PveGroupCoordinator.h"
#include "LFGMgr.h"
#include "Diagnostics/BotInspector.h"
#include "Diagnostics/BotSmokeTest.h"
#include "Diagnostics/CreatureStats.h"
#include "Diagnostics/DamageParity.h"
#include "Diagnostics/SpellAuditFormat.h"
#include "Diagnostics/SpellCheckExpected.h"
#include "Diagnostics/ScenarioRunner.h"
#include "Fleet/BotAccountMgr.h"
#include "Fleet/BotCharacterFactory.h"
#include "Fleet/BotComposition.h"
#include "Fleet/BotIdentityRegistry.h"
#include "Fleet/BotPopulationManager.h"
#include "Fleet/BotSize.h"
#include "Fleet/InstanceGroupCap.h"
#include "Fleet/BotSetupPipeline.h"
#include "Fleet/CompanionDelete.h"
#include "Fleet/OwnerRegistry.h"
#include "Bot/Gear/OwnedGearPolicy.h"
#include "Bot/BotAI.h"
#include "Bot/Formation.h"
#include "Bot/BotIntentEmitter.h"
#include "Threading/IntentQueue.h"
#include "Combat/ApRegistry.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotPersonality.h"
#include "Bot/BotTextMgr.h"
#include "Api/BotLanguage.h"
#include "Bot/BotRng.h"
#include "Session/BotSessionMgr.h"
#include "Threading/TickScheduler.h"
#include "PathGenerator.h"      // /playerbot pathcompare
#include "dtQueryFilterTC.h"    // /playerbot roadstats RoadStats counters
#include "Threading/SnapshotPublisher.h"
#include "Util/ConfigReader.h"
#include "Services.h"
#include "Diagnostics/PerfCounters.h"
#include "Bot/BotTypes.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/Battleground/BattlegroundScript.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotSnapshotView.h"
#include "Session/AddonControl.h"
#include "Session/LazyLock.h"
#include "World/WorldMetadata.h"
#include "ObjectMgr.h"
#include "QuestDef.h"               // .playerbot xpprobe
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "TraitMgr.h"
#include "Containers.h"
#include "MapUtils.h"
#include "DB2Stores.h"
#include "HandcraftedRoadStorage.h"   // .playerbot reloadroutes â road segment reload
#include "HandcraftedRoadGraph.h"     // .playerbot reloadroutes â invalidate road graph cache
#include "TerrainMgr.h"               // .playerbot reloadroutes â clear applied-road cache
#include "GridNotifiers.h"            // .playerbot killentry â AllCreaturesOfEntryInRange
#include "GridNotifiersImpl.h"        // .playerbot killentry â CreatureListSearcher
#include "CellImpl.h"                 // .playerbot killentry â Cell::VisitAllObjects
#include <algorithm>
#include <array>
#include <map>

using namespace Trinity::ChatCommands;

namespace
{
// Resolve a character name to its low GUID (== BotId). Works for both online
// and offline characters by falling back to sCharacterCache. Returns 0 when
// no character with that name exists. This lets GMs run `.playerbot mark
// <name>` against a character that has never logged in â a chicken-and-egg
// fix for the original mark/unmark which required `FindConnectedPlayerByName`.
Playerbot::BotId ResolveCharacterId(std::string name)
{
    // Names are stored as "Name": `.playerbot summon myalt` has to find Myalt.
    if (!normalizePlayerName(name)) return 0;
    if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(name))
        return p->GetGUID().GetCounter();
    ObjectGuid g = sCharacterCache->GetCharacterGuidByName(name);
    return g.IsEmpty() ? 0 : g.GetCounter();
}

// The character name a player typed, normalized ("kar kar" = Kar when Kar's surname is "kar"; Commands/TypedName.h). Empty
// when nothing usable was typed.
std::string TypedCharacterName(std::string_view typed)
{
    std::string name = Playerbot::V2::TypedName::Pick(typed, [](std::string first) -> std::string
    {
        if (!normalizePlayerName(first))
            return {};
        ObjectGuid const g = sCharacterCache->GetCharacterGuidByName(first);
        return g.IsEmpty() ? std::string{} : sCharacterCache->GetCharacterSurnameByGuid(g);
    });
    if (!normalizePlayerName(name))
        return {};
    return name;
}

// Map-object-store visitor collecting CONTROL_ZONE gameobjects for the
// `.playerbot bgzones` diagnostic. File-scope because local classes can't
// carry the template Visit overload TypeContainerVisitor requires.
struct ControlZoneCollector
{
    std::vector<GameObject*> zones;
    template<class T>
    void Visit(std::unordered_map<ObjectGuid, T*>&) { }
    void Visit(std::unordered_map<ObjectGuid, GameObject*>& m)
    {
        for (auto const& [guid, go] : m)
            if (go && go->GetGoType() == GAMEOBJECT_TYPE_CONTROL_ZONE)
                zones.push_back(go);
    }
};

// Lower-case a string in place (no locale, ASCII only â race/class names).
std::string ToLowerAscii(std::string s)
{
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return s;
}

// Parse a race name OR a numeric race id. Returns 0 on unrecognized input.
// Aliases match common short forms: 'elf' -> NightElf, 'be' -> BloodElf, etc.
uint8 ParseRace(std::string const& token)
{
    if (token.empty()) return 0;
    if (std::isdigit(static_cast<unsigned char>(token.front())))
        return uint8(std::strtoul(token.c_str(), nullptr, 10));
    std::string s = ToLowerAscii(token);
    if (s == "human")                         return RACE_HUMAN;
    if (s == "orc")                           return RACE_ORC;
    if (s == "dwarf")                         return RACE_DWARF;
    if (s == "nightelf" || s == "elf")        return RACE_NIGHTELF;
    if (s == "undead"   || s == "forsaken")   return RACE_UNDEAD_PLAYER;
    if (s == "tauren")                        return RACE_TAUREN;
    if (s == "gnome")                         return RACE_GNOME;
    if (s == "troll")                         return RACE_TROLL;
    if (s == "goblin")                        return RACE_GOBLIN;
    if (s == "bloodelf" || s == "be")         return RACE_BLOODELF;
    if (s == "draenei")                       return RACE_DRAENEI;
    if (s == "worgen")                        return RACE_WORGEN;
    if (s == "pandaren") /* alliance default */ return RACE_PANDAREN_ALLIANCE;
    if (s == "pandarena")                     return RACE_PANDAREN_ALLIANCE;
    if (s == "pandarenh")                     return RACE_PANDAREN_HORDE;
    // Legion / BfA allied races
    if (s == "nightborne"  || s == "nb")      return RACE_NIGHTBORNE;
    if (s == "highmountain"|| s == "hmt")     return RACE_HIGHMOUNTAIN_TAUREN;
    if (s == "voidelf"     || s == "ve")      return RACE_VOID_ELF;
    if (s == "lightforged" || s == "ld")      return RACE_LIGHTFORGED_DRAENEI;
    if (s == "zandalari"   || s == "zt")      return RACE_ZANDALARI_TROLL;
    if (s == "kultiran"    || s == "kt")      return RACE_KUL_TIRAN;
    if (s == "darkiron"    || s == "did")     return RACE_DARK_IRON_DWARF;
    if (s == "vulpera")                       return RACE_VULPERA;
    if (s == "magharorc"   || s == "magh")    return RACE_MAGHAR_ORC;
    if (s == "mechagnome"  || s == "mech")    return RACE_MECHAGNOME;
    // Dragonflight / TWW / Midnight
    if (s == "dracthyr"    || s == "dra")     return RACE_DRACTHYR_ALLIANCE; // alliance default
    if (s == "dracthyra")                     return RACE_DRACTHYR_ALLIANCE;
    if (s == "dracthyrh")                     return RACE_DRACTHYR_HORDE;
    if (s == "earthen"     || s == "ear")     return RACE_EARTHEN_DWARF_ALLIANCE; // alliance default
    if (s == "earthena")                      return RACE_EARTHEN_DWARF_ALLIANCE;
    if (s == "earthenh")                      return RACE_EARTHEN_DWARF_HORDE;
    if (s == "haranir"     || s == "harronir") return uint8(86); // RACE_HARRONIR â pending TC enable
    return 0;
}

// Parse a class name OR a numeric class id. 0 on failure.
uint8 ParseClass(std::string const& token)
{
    if (token.empty()) return 0;
    if (std::isdigit(static_cast<unsigned char>(token.front())))
        return uint8(std::strtoul(token.c_str(), nullptr, 10));
    std::string s = ToLowerAscii(token);
    if (s == "warrior")                          return CLASS_WARRIOR;
    if (s == "paladin"   || s == "pala")         return CLASS_PALADIN;
    if (s == "hunter"    || s == "hunt")         return CLASS_HUNTER;
    if (s == "rogue")                            return CLASS_ROGUE;
    if (s == "priest")                           return CLASS_PRIEST;
    if (s == "deathknight" || s == "dk")         return CLASS_DEATH_KNIGHT;
    if (s == "shaman"    || s == "sham")         return CLASS_SHAMAN;
    if (s == "mage")                             return CLASS_MAGE;
    if (s == "warlock"   || s == "lock" || s == "wlock") return CLASS_WARLOCK;
    if (s == "monk")                             return CLASS_MONK;
    if (s == "druid")                            return CLASS_DRUID;
    if (s == "demonhunter" || s == "dh")         return CLASS_DEMON_HUNTER;
    if (s == "evoker"    || s == "evk")          return CLASS_EVOKER;
    return 0;
}

// Static-validation pass for `.playerbot smoketest dungeon`. Walks each
// registered DungeonScript (or one, when `only_map` != 0) and verifies:
//   1. `bosses[]` non-empty (warns when empty â playable without it but
//      tank-advance falls back to boss-Cell scan only).
//   2. Every boss creature_template entry resolves via sObjectMgr.
//   3. Every mandatory_interrupt_spells id resolves via sSpellMgr.
//   4. Every dangerous_auras id resolves via sSpellMgr.
// Counts PASS/WARN/FAIL per script and prints a summary table. Runs
// synchronously on the calling thread â no bots involved.
bool RunDungeonValidation(ChatHandler* handler, uint32 only_map)
{
    auto& mgr = Playerbot::Services::Dungeons();
    if (mgr.size() == 0)
    {
        handler->SendSysMessage("No dungeon scripts registered.");
        return false;
    }
    // Snapshot view requires a real bot, but get_advice via empty arg is
    // not safe. Instead, call the per-script's get_advice with a stub â
    // we only need the raw advice fields, not snapshot-conditional ones.
    // Most scripts ignore the snapshot anyway (their advice is static).
    // For scripts that DO consult the snapshot (rare), we'll miss those
    // branches â acceptable for a static-validation pass.
    uint32 total_scripts = 0;
    uint32 pass_count    = 0;
    uint32 warn_count    = 0;
    uint32 fail_count    = 0;
    std::vector<std::string> fail_lines;
    std::vector<std::string> warn_lines;

    mgr.for_each_script([&](Playerbot::DungeonScript const& script)
    {
        if (only_map != 0 && script.map_id() != only_map) return;
        ++total_scripts;

        // We don't have a real snapshot to pass, but most scripts don't
        // consult the view â so passing a null reference would crash.
        // Use a default-constructed BotSnapshot wrapped in a view. The
        // view's accessors return zero/empty so any code path is safe.
        Playerbot::BotSnapshot empty_snap{};
        Playerbot::BotSnapshotView empty_view(empty_snap);
        Playerbot::DungeonAdvice a = script.get_advice(empty_view);

        uint32 local_warn = 0, local_fail = 0;
        std::string detail;

        if (a.bosses.empty())
        {
            ++local_warn;
            detail += "no_bosses ";
        }
        // Map_id sanity â catches typos (LostCityOfTolvir 754â755 bug
        // class). Map.db2 lookup is cheap.
        if (!sMapStore.LookupEntry(script.map_id()))
        {
            ++local_fail;
            detail += fmt::format("bad_map_id={} ", script.map_id());
        }
        for (uint32 e : a.bosses)
        {
            if (!sObjectMgr->GetCreatureTemplate(e))
            {
                ++local_fail;
                detail += fmt::format("bad_boss_entry={} ", e);
            }
        }
        for (uint32 s : a.mandatory_interrupt_spells)
        {
            if (!sSpellMgr->GetSpellInfo(s, DIFFICULTY_NONE))
            {
                ++local_warn;  // many spell ids are diff-specific; warn not fail
                detail += fmt::format("unk_intr={} ", s);
            }
        }
        for (uint32 s : a.dangerous_auras)
        {
            if (!sSpellMgr->GetSpellInfo(s, DIFFICULTY_NONE))
            {
                ++local_warn;
                detail += fmt::format("unk_aura={} ", s);
            }
        }

        if (local_fail > 0)
        {
            ++fail_count;
            fail_lines.push_back(fmt::format("[FAIL] {} (map={}): {}",
                script.name(), script.map_id(), detail));
        }
        else if (local_warn > 0)
        {
            ++warn_count;
            warn_lines.push_back(fmt::format("[WARN] {} (map={}): {}",
                script.name(), script.map_id(), detail));
        }
        else
        {
            ++pass_count;
        }
    });

    if (total_scripts == 0)
    {
        handler->PSendSysMessage(
            "No script found for map_id=%u. Use `.playerbot smoketest dungeon all` "
            "to list all registered dungeons.", only_map);
        return false;
    }

    handler->PSendSysMessage(
        "==== Dungeon validation: %u scripts | PASS=%u WARN=%u FAIL=%u ====",
        total_scripts, pass_count, warn_count, fail_count);
    for (auto const& l : fail_lines) handler->SendSysMessage(l);
    for (auto const& l : warn_lines) handler->SendSysMessage(l);
    if (fail_count == 0 && warn_count == 0)
        handler->SendSysMessage("All scripts clean.");

    return fail_count == 0;
}

// Static-validation pass for `.playerbot smoketest bg`. Walks each
// registered BattlegroundScript and verifies:
//   1. role_by_slot non-empty (else the bot never gets a job).
//   2. role_by_slot does not contain only Free / silent entries.
//   3. Coords are non-NaN and in plausible ranges (|x|,|y| < 20000 â TC's
//      map bounds; |z| < 1500 â Hyjal top elevation).
//   4. If endgame_creature_entry set, the entry resolves via sObjectMgr.
//   5. If vehicle_creature_entries set, each resolves via sObjectMgr.
//   6. If FlagCarrier role present, auto_use_go_types non-empty (the bot
//      needs a pickup mechanism â modern NEW_FLAG (36) etc.).
//   7. Nodes with follow_creature_entry set must resolve to a real entry.
//   8. Per-vehicle seat-spell IDs (vehicle_seat_spell + by_entry) must
//      resolve via sSpellMgr.
//   9. AliasToBaseBg coverage: walk a curated set of BattlemasterList
//      IDs from SharedDefines and assert TryGetScriptFor returns non-null.
// Runs synchronously on the calling thread â no bots involved.
bool RunBgValidation(ChatHandler* handler, uint32 only_id)
{
    auto& mgr = Playerbot::Services::Battlegrounds();
    if (mgr.size() == 0)
    { handler->SendSysMessage("No battleground scripts registered."); return false; }

    uint32 total_scripts = 0, pass_count = 0, warn_count = 0, fail_count = 0;
    std::vector<std::string> fail_lines, warn_lines;

    auto coord_sane = [](float x, float y, float z) -> bool {
        if (std::isnan(x) || std::isnan(y) || std::isnan(z)) return false;
        if (std::fabs(x) > 20000.0f) return false;
        if (std::fabs(y) > 20000.0f) return false;
        if (std::fabs(z) > 1500.0f)  return false;
        return true;
    };

    mgr.for_each_script([&](Playerbot::BattlegroundScript const& script)
    {
        if (only_id != 0 && script.bg_type_id() != only_id) return;
        ++total_scripts;

        // Both faction passes â most scripts branch on s.is_horde().
        // We don't have a real BotSnapshotView; an empty view's accessors
        // return zero/false safely. Note: the script may produce
        // faction-symmetric advice; we accept that.
        Playerbot::BotSnapshot empty_snap{};
        Playerbot::BotSnapshotView empty_view(empty_snap);
        Playerbot::BattlegroundAdvice a = script.get_advice(empty_view);

        uint32 local_warn = 0, local_fail = 0;
        std::string detail;

        if (a.role_by_slot.empty())
        { ++local_fail; detail += "no_roles "; }
        else
        {
            bool any_real = false;
            for (auto r : a.role_by_slot) if (r != Playerbot::BgRole::Free) { any_real = true; break; }
            if (!any_real) { ++local_warn; detail += "all_roles_free "; }
        }

        if ((a.enemy_flag_x != 0.f || a.enemy_flag_y != 0.f) &&
            !coord_sane(a.enemy_flag_x, a.enemy_flag_y, a.enemy_flag_z))
        { ++local_fail; detail += "bad_enemy_flag_coord "; }
        if ((a.own_flag_x != 0.f || a.own_flag_y != 0.f) &&
            !coord_sane(a.own_flag_x, a.own_flag_y, a.own_flag_z))
        { ++local_fail; detail += "bad_own_flag_coord "; }
        if ((a.home_base_x != 0.f || a.home_base_y != 0.f) &&
            !coord_sane(a.home_base_x, a.home_base_y, a.home_base_z))
        { ++local_fail; detail += "bad_home_base_coord "; }
        if ((a.endgame_target_x != 0.f || a.endgame_target_y != 0.f) &&
            !coord_sane(a.endgame_target_x, a.endgame_target_y, a.endgame_target_z))
        { ++local_fail; detail += "bad_endgame_coord "; }

        // FC role requires a pickup mechanism (auto_use_go_types) OR live
        // flag-carrier detection via auras. Modern CTF needs NEW_FLAG (36).
        bool has_fc_role = false;
        for (auto r : a.role_by_slot)
            if (r == Playerbot::BgRole::FlagCarrier || r == Playerbot::BgRole::OrbCarrier)
            { has_fc_role = true; break; }
        if (has_fc_role && a.auto_use_go_types.empty())
        { ++local_warn; detail += "fc_role_no_pickup_types "; }

        // Endgame creature must resolve. Stub get_advice on horde-stub
        // gave us one side's entry; we accept either side. If non-zero
        // and the lookup fails, that's a fail.
        if (a.endgame_creature_entry != 0 &&
            !sObjectMgr->GetCreatureTemplate(a.endgame_creature_entry))
        { ++local_fail; detail += fmt::format("bad_endgame_entry={} ",
                                              a.endgame_creature_entry); }

        for (uint32_t e : a.vehicle_creature_entries)
        {
            if (!sObjectMgr->GetCreatureTemplate(e))
            { ++local_fail; detail += fmt::format("bad_vehicle_entry={} ", e); }
        }

        if (a.vehicle_seat_spell != 0 &&
            !sSpellMgr->GetSpellInfo(a.vehicle_seat_spell, DIFFICULTY_NONE))
        { ++local_warn; detail += fmt::format("unk_seat_spell={} ",
                                              a.vehicle_seat_spell); }
        for (auto const& [entry, spell] : a.vehicle_seat_spell_by_entry)
        {
            if (!sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE))
            { ++local_warn; detail += fmt::format("unk_seat_spell_{}={} ", entry, spell); }
        }

        // Nodes â coord sanity + follow_creature_entry resolution.
        for (auto const& n : a.nodes)
        {
            if ((n.x != 0.f || n.y != 0.f) && !coord_sane(n.x, n.y, n.z))
            { ++local_fail; detail += fmt::format("bad_node_coord({}) ", n.name); }
            if (n.follow_creature_entry != 0 &&
                !sObjectMgr->GetCreatureTemplate(n.follow_creature_entry))
            { ++local_fail; detail += fmt::format("bad_follow_entry={} ", n.follow_creature_entry); }
        }

        // fc_class_preference class-ids must be in [1, 13] (Warrior..Evoker).
        // A typo (e.g. 14, 0) would silently no-op without this check.
        for (uint8_t c : a.fc_class_preference)
            if (c == 0 || c > 13)
            { ++local_warn; detail += fmt::format("bad_fc_class={} ", c); }

        if (local_fail > 0)
        {
            ++fail_count;
            fail_lines.push_back(fmt::format("[FAIL] {} (id={}): {}",
                script.name(), script.bg_type_id(), detail));
        }
        else if (local_warn > 0)
        {
            ++warn_count;
            warn_lines.push_back(fmt::format("[WARN] {} (id={}): {}",
                script.name(), script.bg_type_id(), detail));
        }
        else
        {
            ++pass_count;
        }
    });

    if (total_scripts == 0)
    {
        handler->PSendSysMessage(
            "No script found for bg_type_id=%u. Use `.playerbot smoketest bg all` "
            "to list all registered BGs.", only_id);
        return false;
    }

    // Alias-coverage assertion when running 'all'. Curated list of
    // BattlemasterList IDs from SharedDefines.h â excludes queue
    // meta-IDs (RB 32, RATED_* 100..102, RANDOM_EPIC 901, RANDOM_BG 1029)
    // and PvE / open-world IDs (SS_VS_TM 789, CI 887) which legitimately
    // have no bot script.
    uint32 uncovered = 0;
    std::vector<std::string> uncovered_lines;
    if (only_id == 0)
    {
        static constexpr uint16_t kAllBgIds[] = {
              1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
             11,  30, 108, 120, 699, 708, 719, 754, 757, 803,
            808, 816, 846, 847, 849, 853, 857, 858, 859, 860,
            861, 862, 866, 868, 869, 870, 871, 872, 873, 874,
            875, 879, 880, 881, 882, 883, 884, 885, 886, 890,
            894, 897, 902, 903, 904, 905, 906, 907, 908, 909,
            910, 911, 912, 913,
            1014, 1017, 1018, 1019, 1020, 1021, 1022, 1025,
            1030, 1033, 1036, 1037, 1039, 1041
        };
        for (uint16_t bid : kAllBgIds)
        {
            if (mgr.TryGetScriptFor(bid) == nullptr)
            {
                ++uncovered;
                uncovered_lines.push_back(fmt::format("id={}", bid));
            }
        }
    }

    handler->PSendSysMessage(
        "==== BG validation: %u scripts | PASS=%u WARN=%u FAIL=%u | uncovered_ids=%u ====",
        total_scripts, pass_count, warn_count, fail_count, uncovered);
    for (auto const& l : fail_lines) handler->SendSysMessage(l);
    for (auto const& l : warn_lines) handler->SendSysMessage(l);
    if (uncovered > 0)
    {
        handler->PSendSysMessage("Uncovered BattlemasterList ids:");
        std::string joined;
        for (size_t i = 0; i < uncovered_lines.size(); ++i)
        {
            if (i) joined += ", ";
            joined += uncovered_lines[i];
        }
        handler->SendSysMessage(joined);
    }
    if (fail_count == 0 && warn_count == 0 && uncovered == 0)
        handler->SendSysMessage("All BG scripts clean and every BML id covered.");

    return fail_count == 0 && uncovered == 0;
}

} // anonymous namespace

class playerbot_v2_commandscript : public CommandScript
{
public:
    playerbot_v2_commandscript() : CommandScript("playerbot_v2_commandscript") {}

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable v2Table =
        {
            { "inspect",       HandleInspect,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            // Dev checks that need no client logged in (GM console / SOAP): can a player's bags take an item, a craft order as if
            // a player whispered it, and a line to a bot through the bot bridge as if said in party.
            { "canstore",      HandleCanStore,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "craftfor",      HandleCraftFor,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bridgeask",     HandleBridgeAsk,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "status",        HandleStatus,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "activity",      HandleActivity,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "levels",        HandleLevels,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "maps",          HandleMaps,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "quests",        HandleQuests,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "crafting",      HandleCrafting,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "stuck",         HandleStuck,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "flips",         HandleFlips,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bg",            HandleBg,            rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "mark",          HandleMark,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "unmark",        HandleUnmark,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "adopt",         HandleAdopt,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "disown",        HandleDisown,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "owner",         HandleOwner,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "squadsave",     HandleSquadSave,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "squadload",     HandleSquadLoad,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "list",          HandleList,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "count",         HandleCount,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "login",         HandleLogin,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "logout",        HandleLogout,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "loginall",      HandleLoginAll,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "logoutall",     HandleLogoutAll,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reload",        HandleReload,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "create",        HandleCreate,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "spawn",         HandleSpawn,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dist",          HandleDist,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "summonall",     HandleSummonAll,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "levelall",      HandleLevelAll,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "delete",        HandleDelete,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "level",         HandleLevel,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "questrun",      HandleQuestRun,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reset",         HandleReset,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "money",         HandleMoney,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "goto",          HandleGoto,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "listrotations", HandleListRotations, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "resetstats",    HandleResetStats,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "population",    HandlePopulation,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pipeline",      HandlePipeline,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "health",        HandleHealth,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "fleethealth",   HandleFleetHealth,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgstats",       HandleBgStats,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgcoord",       HandleBgCoord,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgzones",       HandleBgZones,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgjoin",        HandleBgJoin,        rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "bgenter",       HandleBgEnter,       rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "bgleave",       HandleBgLeave,       rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "pvecoord",      HandlePveCoord,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "group",         HandleGroup,         rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeontest",   HandleDungeonTest,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeonreset",  HandleDungeonReset,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "dungeondump",   HandleDungeonDump,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reloadroutes",  HandleReloadRoutes,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "killentry",     HandleKillEntry,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bginfo",        HandleBgInfo,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "diag",          HandleDiag,          rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "wedges",        HandleWedges,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "reseed",        HandleReseed,        rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "smoketest",     HandleSmoketest,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "scenario",      HandleScenario,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "creaturestats", HandleCreatureStats, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "spellaudit",    HandleSpellAudit,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "spellcheck",    HandleSpellCheck,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "auracheck",     HandleAuraCheck,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "proccheck",     HandleProcCheck,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "casttest",      HandleCastTest,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "xpprobe",       HandleXpProbe,       rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "formprobe",     HandleFormProbe,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "campprobe",     HandleCampProbe,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "spotprobe",     HandleSpotProbe,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "collisionprobe", HandleCollisionProbe, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "langprobe",     HandleLangProbe,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "learncheck",    HandleLearnCheck,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "traitcheck",    HandleTraitCheck,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "talentwalk",    HandleTalentWalk,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "graveyardscan", HandleGraveyardScan, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "wipefleet",     HandleWipeFleet,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "roadstats",     HandleRoadStats,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "roadreset",     HandleRoadReset,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "pathcompare",   HandlePathCompare,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "terrainprofile",HandleTerrainProfile,rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "meta",          HandleMeta,          rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            // Player-facing â gated to RBAC_PERM_JOIN_NORMAL_BG (the
            // "is a regular player" perm every account has by default)
            // so a normal user can summon their own alts without GM
            // rights. Account-match check inside the handler still
            // prevents cross-account abuse, and PlayerbotV2.MaxAltsAsBots
            // caps total simultaneously-owned alts per account.
            { "summon",        HandleSummon,        rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "self",          HandleSelf,          rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "lazy",          HandleLazy,          rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
        };
        // One-step companions for the player at the keyboard: `.bot add warrior tank` creates the bot, binds it to the
        // caller's character, levels it to them, teaches its spells, sets role/talents/gear, brings it over and groups it.
        // Every player: the commands for the caller's own bots (each one only ever touches bots owned by the caller's account).
        // GM only: world, bgstress, bgsoak and itemstats, which look at other players or the whole server.
        static ChatCommandTable botTable =
        {
            { "about",         HandleBotAbout,      rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::Yes },
            { "addon",         HandleBotAddonHello, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "add",           HandleBotAdd,        rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "lfg",           HandleBotLfg,        rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "dungeon",       HandleBotDungeon,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "call",          HandleBotCall,       rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            // Every player: brings the caller's own bots only (`.bot summon all|saved|<bot name>`).
            { "summon",        HandleBotSummon,     rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "dismiss",       HandleBotDismiss,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "regear",        HandleBotRegear,     rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "gear",          HandleBotGear,       rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "itemstats",     HandleBotItemStats,  rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "save",          HandleBotSave,       rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "unsave",        HandleBotUnsave,     rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "delete",        HandleBotDelete,     rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "list",          HandleBotList,       rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "language",      HandleBotLanguage,   rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No  },
            { "world",         HandleBotWorld,      rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            // GM only: how many world bots the realm has (Off / Low / Medium / High), set in game until the next restart (the launcher's Bots setting decides at every start).
            { "population",    HandleBotPopulation, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgstress",      HandleBotBgStress,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "bgsoak",        HandleBotBgSoak,     rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
        };
        static ChatCommandTable root =
        {
            { "playerbot", v2Table },
            { "bot",       botTable },
            // `.w <name> <text>` / `.tell <name> <text>`: whisper as a chat command. The 70205 client has no working /w, and inside
            // battlegrounds the addon's whisper is blocked (Karrz 2026-10-08, WSG: SAY packets and dot commands go through,
            // whispers never leave the client). Every player.
            { "w",    HandleWhisperCmd, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "tell", HandleWhisperCmd, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            // Added to the core's `.learn all` table.
            { "learn all mylevel", HandleLearnAllMyLevelCommand, rbac::RBAC_PERM_COMMAND_LEARN_ALL_MY_SPELLS, Console::No },
        };
        return root;
    }

    // `.playerbot canstore <player> <item id> [count]`: what the game answers when that many of the item would go into the player's
    // bags (the check a conjure or craft makes first), with the item's limits and the free bag slots.
    static bool HandleCanStore(ChatHandler* handler, std::string name, uint32 item, Optional<uint32> count)
    {
        Player* p = ObjectAccessor::FindConnectedPlayerByName(name);
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(item);
        if (!p || !t)
        {
            handler->SendSysMessage(!p ? "No such player online." : "No such item.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        uint32 const n = std::max<uint32>(1, count.value_or(1));
        ItemPosCountVec dest;
        uint32 no_space = 0;
        InventoryResult const r = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item, n, &no_space);
        handler->PSendSysMessage("%s, item %u (%s) x%u: InventoryResult %u%s; limit category %u, max count %d, stack %u, carried %u, "
                                 "free bag slots %u, no room for %u",
            p->GetName().c_str(), item, t->GetName(DEFAULT_LOCALE), n, uint32(r), r == EQUIP_ERR_OK ? " (fits)" : "",
            t->GetItemLimitCategory(), t->GetMaxCount(), t->GetMaxStackSize(), p->GetItemCount(item, true), p->GetFreeInventorySlotCount(),
            no_space);
        return true;
    }

    // `.playerbot craftfor <requester> <bot> <order>`: the bot takes a craft order ("craft water 10") from `requester` as if he had
    // whispered it (no rights check: a test of the craft path itself). The bot's whispers go to the requester and the log.
    static bool HandleCraftFor(ChatHandler* handler, std::string requester, std::string bot, Tail text)
    {
        Player* r = ObjectAccessor::FindConnectedPlayerByName(requester);
        Player* b = ObjectAccessor::FindConnectedPlayerByName(bot);
        std::string order(text);
        if (!r || !b || order.empty())
        {
            handler->SendSysMessage("Usage: .playerbot craftfor <requester> <bot> craft <item> [count] (both online)");
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (order.rfind("craft ", 0) != 0)
            order = "craft " + order;
        bool const taken = Playerbot::V2::PlayerCraftOrders::Order(r, b, order);
        handler->PSendSysMessage("%s: \"%s\" %s (see the [craft_order] lines)", b->GetName().c_str(), order.c_str(),
                                 taken ? "read as a craft order" : "not a craft order");
        return true;
    }

    // `.playerbot bridgeask <from> <bot> <text>`: a chat_in to the bot bridge's program as if `from` had said it in party, so the
    // program's answer and action can be tested without a client. Needs the bridge on and its program answering.
    static bool HandleBridgeAsk(ChatHandler* handler, std::string from, std::string bot, Tail text)
    {
        Player* f = ObjectAccessor::FindConnectedPlayerByName(from);
        Player* b = ObjectAccessor::FindConnectedPlayerByName(bot);
        std::string line(text);
        if (!f || !b || line.empty())
        {
            handler->SendSysMessage("Usage: .playerbot bridgeask <from> <bot> <text> (both online)");
            handler->SetSentErrorMessage(true);
            return false;
        }
        uint64 const id = Playerbot::BotBridge::ReportChat(b, f, "party", line, "");
        handler->PSendSysMessage(id ? "chat_in %llu written for %s" : "not written (bridge off, or no program answering)",
                                 (unsigned long long)id, b->GetName().c_str());
        return true;
    }

    static bool HandleWhisperCmd(ChatHandler* handler, std::string name, Tail text)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        WorldSession* sess = handler->GetSession();
        if (!sess || !sess->GetPlayer())
            return false;
        std::string msg(text);
        if (name.empty() || msg.empty())
        {
            handler->SendSysMessage("Usage: .w <name> <message>");
            handler->SetSentErrorMessage(true);
            return false;
        }
        // The sender's own faction language (a whisper in LANG_UNIVERSAL is refused as a hacking attempt); HandleChatMessage turns
        // it into LANG_UNIVERSAL for GMs / two-side chat like a client whisper, and prints "No player named ..." itself.
        Language const lang = sess->GetPlayer()->GetTeam() == ALLIANCE ? LANG_COMMON : LANG_ORCISH;
        sess->HandleChatMessage(CHAT_MSG_WHISPER, lang, std::move(msg), std::move(name));
        return true;
    }

    // ---- .bot: one-step companions ------------------------------------------------------------------------------

    static Playerbot::Role ParseRoleWord(std::string const& token, bool& ok)
    {
        std::string const s = ToLowerAscii(token);
        ok = true;
        if (s == "tank")                                    return Playerbot::Role::Tank;
        if (s == "heal" || s == "healer")                   return Playerbot::Role::Healer;
        if (s == "dps" || s == "damage")                    return Playerbot::Role::Dps;
        ok = false;
        return Playerbot::Role::Unknown;
    }

    // Classic 1.60 races only: the client offers no others.
    static bool IsClassicRace(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN: case RACE_ORC: case RACE_DWARF: case RACE_NIGHTELF:
            case RACE_UNDEAD_PLAYER: case RACE_TAUREN: case RACE_GNOME: case RACE_TROLL:
                return true;
            default:
                return false;
        }
    }

    // The caller's own race when it can be this class, else a random race of the caller's faction that can.
    static uint8 PickCompanionRace(Player const* owner, uint8 cls)
    {
        // Legal on this realm, not merely present in the retail-derived PlayerInfo: a Gnome/Undead owner has
        // PlayerInfo for a hunter but no Forever hunter, and BotComposition::Roll refuses it.
        using Playerbot::V2::BotComposition;
        if (BotComposition::IsLegalRaceClass(owner->GetRace(), cls))
            return owner->GetRace();
        std::vector<uint8> races;
        for (uint8 race : { RACE_HUMAN, RACE_ORC, RACE_DWARF, RACE_NIGHTELF, RACE_UNDEAD_PLAYER, RACE_TAUREN, RACE_GNOME, RACE_TROLL })
            if (Player::TeamForRace(race) == owner->GetTeam() && BotComposition::IsLegalRaceClass(race, cls))
                races.push_back(race);
        return races.empty() ? 0 : races[urand(0, uint32(races.size() - 1))];
    }

    // A refusal the player has already been told about: flags the message as the error so the command framework does not
    // append its generic "### USAGE ... no detailed usage information" text after it (Karrz, 2026-10-08).
    static bool Refuse(ChatHandler* handler)
    {
        handler->SetSentErrorMessage(true);
        return false;
    }

    // .bot addon: sent silently by the ForeverBots add-on a few seconds after login, so the server knows this client has it and
    // does not send the download hint (Playerbot.AddonHint.*). Says nothing.
    static bool HandleBotAddonHello(ChatHandler* handler)
    {
        WorldSession* sess = handler ? handler->GetSession() : nullptr;
        if (sess && sess->GetPlayer())
            Playerbot::V2::Module::instance().NoteAddonHello(sess->GetPlayer());
        return true;
    }

    // .bot about: the name of the bots, so it shows in the command list and `.bot` help
    // .bot language [auto|en|ru]: the language your bots answer you in (whispers, group chat, these command replies). auto, the start
    // setting, follows the game client's language; the choice is kept per account (Api/BotLanguage.h).
    static bool HandleBotLanguage(ChatHandler* handler, Optional<std::string> word)
    {
        namespace Lang = Playerbot::BotLanguage;
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me)
            return false;
        if (word)
        {
            Lang::Pref pref = Lang::Pref::Auto;
            if (!Lang::ParsePref(*word, pref))
            {
                Lang::LocalizedHandler localized(handler);
                localized.Get()->SendSysMessage("Usage: .bot language auto|en|ru");
                handler->SetSentErrorMessage(true);
                return false;
            }
            Lang::SetPref(sess->GetAccountId(), pref);
        }
        // Taken after the change, so the answer is already in the language just chosen.
        Lang::LocalizedHandler localized(handler); handler = localized.Get();
        bool const russian = Lang::LangOf(me) == Playerbot::Locale::Lang::Ru;
        handler->PSendSysMessage("Bot replies: %s (%s). .bot language auto|en|ru changes it.", russian ? "Russian" : "English",
            Lang::GetPref(sess->GetAccountId()) == Lang::Pref::Auto ? "your client's language" : "your choice");
        return true;
    }

    static bool HandleBotAbout(ChatHandler* handler)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        handler->SendSysMessage("WoW Forever Bots by Karrz, based on agatho's PlayerbotV2 (GPL-2.0). .bot add <class> [tank|healer|dps], .bot call, .bot dismiss; all orders: BOT_COMMANDS.md");
        return true;
    }

    // .bot lfg [tank|healer|dps]: fill the group's missing tank / healer / damage places now with level-matched companions
    // (the Group Finder does the same by itself when nobody joins a posted listing in time). The word is your own role.
    static bool HandleBotLfg(ChatHandler* handler, Optional<std::string> ownRoleArg)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me || !Playerbot::Services::Initialized())
        { handler->SendSysMessage("`.bot lfg` is in-game only."); return Refuse(handler); }
        Playerbot::Role own = Playerbot::Role::Unknown;
        if (ownRoleArg)
        {
            bool ok = false;
            own = ParseRoleWord(*ownRoleArg, ok);
            if (!ok)
            { handler->SendSysMessage("Usage: .bot lfg [tank|healer|dps] (your own role)"); return Refuse(handler); }
        }
        std::string msg;
        bool const ok = Playerbot::V2::LfgFillNow(me, own, msg);
        handler->SendSysMessage(msg.c_str());
        return ok ? true : Refuse(handler);
    }

    // .bot dungeon [list | <name|any> [tank|healer|dps] | leave | cancel]: the bot dungeon finder (DungeonFinder.cpp). A dungeon (or any
    // for your level) is picked, the missing places are filled with your companions and the group is put inside its door; leave
    // brings you back to where you queued. The role word is your own role.
    static bool HandleBotDungeon(ChatHandler* handler, Optional<std::string> what, Optional<std::string> ownRoleArg)
    {
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me || !Playerbot::Services::Initialized())
        { handler->SendSysMessage("`.bot dungeon` is in-game only."); return Refuse(handler); }
        std::string const word = what ? ToLowerAscii(*what) : std::string("list");
        std::string msg;
        bool ok = true;
        if (word == "list")
            msg = Playerbot::V2::DungeonFinderList(me);
        else if (word == "leave")
            ok = Playerbot::V2::DungeonFinderLeave(me, msg);
        else if (word == "cancel")
            ok = Playerbot::V2::DungeonFinderCancel(me, msg);
        else
        {
            Playerbot::Role own = Playerbot::Role::Unknown;
            if (ownRoleArg)
            {
                bool roleOk = false;
                own = ParseRoleWord(*ownRoleArg, roleOk);
                if (!roleOk)
                { handler->SendSysMessage("Usage: .bot dungeon <name|any> [tank|healer|dps] (your own role); .bot dungeon list|leave|cancel"); return Refuse(handler); }
            }
            ok = Playerbot::V2::DungeonFinderQueue(me, *what, own, msg);
        }
        if (!msg.empty())
            handler->SendSysMessage(msg.c_str());
        return ok ? true : Refuse(handler);
    }

    // .bot add <class> [tank|healer|dps] [race] [male|female]
    static bool HandleBotAdd(ChatHandler* handler, std::string const& class_arg, Optional<std::string> arg2,
                             Optional<std::string> arg3, Optional<std::string> arg4)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return Refuse(handler); }
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me)
        { handler->SendSysMessage("`.bot add` is in-game only."); return Refuse(handler); }

        // Say so up front: otherwise a full raid creates a character that can never join and the owner sees nothing.
        if (Group const* ownerGroup = me->GetGroup(); ownerGroup && ownerGroup->isRaidGroup() && ownerGroup->IsFull())
        { handler->SendSysMessage("Bot add failed: your raid is full."); return Refuse(handler); }
        // Open world: 5 companions out at most (invite world bots for more); inside an instance what the map holds
        // (Playerbot.Behavior.InstanceGroupCap).
        if (std::string full; Playerbot::V2::Fleet::CompanionSpawnFull(me, 0, full))
        {
            std::string capMap;
            TC_LOG_INFO("playerbot.v2", "[InstanceCap] {} .bot add refused: map={} cap={} out={} group={}: {}", me->GetName(), me->GetMapId(),
                        Playerbot::V2::Fleet::InstanceGroupCapFor(me, &capMap), Playerbot::V2::Fleet::CompanionsOut(me),
                        me->GetGroup() ? me->GetGroup()->GetMembersCount() : 1u, full);
            handler->PSendSysMessage("Bot add failed: %s.", full.c_str());
            return Refuse(handler);
        }

        uint8 const cls = ParseClass(class_arg);
        if (!cls || !sChrClassesStore.LookupEntry(cls))
        {
            handler->SendSysMessage("WoW Forever Bots. Usage: .bot add <class> [tank|healer|dps] [race] [male|female]  e.g. .bot add priest healer");
            return Refuse(handler);
        }

        // The optional words can come in any order: a role, a race, a gender.
        Playerbot::Role role = Playerbot::Role::Unknown;
        uint8 race = 0;
        uint8 gender = uint8(urand(GENDER_MALE, GENDER_FEMALE));
        bool anyClass = false;
        for (Optional<std::string> const* arg : { &arg2, &arg3, &arg4 })
        {
            if (!*arg)
                continue;
            std::string const word = ToLowerAscii(**arg);
            bool is_role = false;
            Playerbot::Role const r = ParseRoleWord(word, is_role);
            if (is_role) { role = r; continue; }
            // "anyclass": the add-on's Quick party picks the classes itself, so with hired bots any free one of the role may come.
            if (word == "anyclass")                { anyClass = true; continue; }
            if (word == "male" || word == "m")     { gender = GENDER_MALE; continue; }
            if (word == "female" || word == "f")   { gender = GENDER_FEMALE; continue; }
            if (uint8 const parsed = ParseRace(word); parsed && IsClassicRace(parsed)) { race = parsed; continue; }
            handler->PSendSysMessage("Don't know '%s': expected tank, healer, dps, a race, male or female.", word.c_str());
            return Refuse(handler);
        }

        std::string outMsg;
        bool const ok = AddCompanionFor(me, sess, cls, role, race, gender, class_arg, outMsg,
                                        Playerbot::V2::Fleet::Hire::Grade::None, /*finder*/ false, anyClass);
        if (!outMsg.empty())   // a hired companion that was called instead has said so itself
            handler->SendSysMessage(outMsg.c_str());
        return ok ? true : Refuse(handler);
    }

    // A companion of `me`'s account that is not out (not in the group, not called a moment ago) for `role`, the same class first:
    // in hired mode it comes instead of a new bot. 0 = none. Another class of the same role comes only with `anyClass` (Quick
    // party): a player who picked a class gets that class (the owner 2026-11-09: Shaman and Paladin bots picked in the add-on,
    // Priests, Warriors and Druids came instead).
    static Playerbot::BotId IdleHiredCompanion(Player* me, WorldSession* sess, uint8 cls, Playerbot::Role role, bool anyClass)
    {
        using Playerbot::V2::Fleet::HireMgr;
        uint32 const now = getMSTime();
        Group const* group = me->GetGroup();
        Playerbot::BotId fallback = 0;
        for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
            if ((group && group->IsMember(guid)) || HireMgr::Instance().RecentlyCalled(id, now) || HireMgr::Instance().IsLoaned(id))
                continue;   // (a Group Finder loan bot only plays inside the group it was made for)
            if (Player const* online = ObjectAccessor::FindConnectedPlayer(guid); online && online->GetGroup())
                continue;   // out with someone else of the account
            // A companion bound to another character of the account stays with that character: called for this one it joined, the
            // group check (CompanionGroupHygiene) found its owner character missing and sent it away a second later, and it came at
            // that character's level (the owner 2026-11-07: his main's level 60 companions for his level 33 alt).
            if (uint64 const ownerChar = Playerbot::Services::Owners().GetOwner(id).player_guid; ownerChar != 0 && ownerChar != me->GetGUID().GetCounter())
                continue;
            Playerbot::Role const declared = Playerbot::Services::Owners().DeclaredRole(id);
            if (role != Playerbot::Role::Unknown && declared != role)
                continue;
            CharacterCacheEntry const* entry = sCharacterCache->GetCharacterCacheByGuid(guid);
            if (!entry)
                continue;
            // A companion above the caller's level would come at its own level (bots are levelled up to the owner, never down): a
            // fitting one is hired instead (the owner 2026-11-07: an account-wide level 60 companion joined his level 33 character).
            if (entry->Level > me->GetLevel())
                continue;
            if (entry->Class == cls)
                return id;
            if (anyClass && !fallback && role != Playerbot::Role::Unknown)
                fallback = id;
        }
        return fallback;
    }

    // Creates one companion of `cls` (race 0 = pick one) for `me` and logs it in; it joins the owner's group through the
    // normal companion setup. Shared by `.bot add` and the Group Finder bot fill. `outMsg` is what the owner is told.
    static bool AddCompanionFor(Player* me, WorldSession* sess, uint8 cls, Playerbot::Role role, uint8 race, uint8 gender,
                                std::string const& className, std::string& outMsg,
                                Playerbot::V2::Fleet::Hire::Grade hireGrade = Playerbot::V2::Fleet::Hire::Grade::None,
                                bool finder = false, bool anyClass = false)
    {
        namespace Hire = Playerbot::V2::Fleet::Hire;
        using Playerbot::V2::Fleet::HireMgr;
        HireMgr& hire = HireMgr::Instance();
        bool const loan = finder && hireGrade == Hire::Grade::None && hire.MustHire(me);
        if (!loan && hireGrade == Hire::Grade::None && hire.MustHire(me))
        {
            if (Playerbot::BotId const idle = IdleHiredCompanion(me, sess, cls, role, anyClass))
            {
                hire.NoteCalled(idle, getMSTime());
                ChatHandler handler(sess);
                outMsg.clear();
                return CallCompanions(&handler, CompanionName(idle), false);
            }
            if (!hire.NearbyBroker(me, Hire::Grade::Sellsword))
            {
                char const* const roleWord = role == Playerbot::Role::Tank ? "tank" : role == Playerbot::Role::Healer ? "healer" : "fighter";
                // A picked class is named: "no hired shaman healer free", not "no hired healer free" while a Priest healer sits idle.
                std::string const what = anyClass ? std::string(roleWord)
                    : className + (role == Playerbot::Role::Unknown ? "" : role == Playerbot::Role::Dps ? " dps" : " " + std::string(roleWord));
                outMsg = Trinity::StringFormat("Hired bots only on this realm: you have no hired {} free. Hire one from a Sellsword Captain at an inn or in a capital city (the Group Finder and the dungeon finder still bring bots).", what);
                return false;
            }
            hireGrade = Hire::Grade::Sellsword;
        }
        uint64 price = 0;
        if (hireGrade != Hire::Grade::None)
        {
            if (!hire.NearbyBroker(me, hireGrade))
            { outMsg = Trinity::StringFormat("There is nobody here who hires out a {}.", Hire::GradeName(hireGrade)); return false; }
            price = HireMgr::PriceHere(me, hireGrade);
            if (!price)
            { outMsg = Trinity::StringFormat("A {} only signs on with a level {} hero.", Hire::GradeName(hireGrade), uint32(Playerbot::MaxPlayerLevel())); return false; }
            if (uint32 const max = HireMgr::MaxPerAccount(); max && Playerbot::Services::Owners().BotsOwnedBy(sess->GetAccountId()).size() >= max)
            { outMsg = Trinity::StringFormat("You have {} bots already, the most this realm allows. End a contract first.", max); return false; }
            if (uint32 const max = HireMgr::LegendMaxPerAccount(); hireGrade == Hire::Grade::Legend && max && hire.CountGrade(sess->GetAccountId(), Hire::Grade::Legend) >= max)
            { outMsg = Trinity::StringFormat("The cartel hires out {} Legend(s) to one hero at a time.", max); return false; }
            if (!me->HasEnoughMoney(price))
            { outMsg = Trinity::StringFormat("You need {} to hire a {}.", Hire::MoneyText(price), Hire::GradeName(hireGrade)); return false; }
        }

        if (race && !Playerbot::V2::BotComposition::IsLegalRaceClass(race, cls))
        { outMsg = "That race can't be a " + className + "."; return false; }
        if (race && Player::TeamForRace(race) != me->GetTeam())
        { outMsg = "That race is from the other faction, so it couldn't join your group."; return false; }
        if (!race)
            race = PickCompanionRace(me, cls);
        if (!race)
        { outMsg = "No race of your faction can be a " + className + "."; return false; }

        auto const pick = Playerbot::V2::BotComposition::Roll(race, cls, gender, "");
        if (pick.race == 0 || pick.name.empty())
        {
            outMsg = Trinity::StringFormat("Bot add failed: couldn't roll a {} (race {}, no name or no legal race/class). See Playerbot.log '[BotComposition] Roll exhausted'.",
                                           className, uint32(race));
            return false;
        }

        auto const created = Playerbot::V2::BotCharacterFactory::Create(sess, pick.name, race, cls, gender);
        if (!created.ok)
        { outMsg = "Couldn't create the bot: " + created.reason; return false; }

        Playerbot::BotId const id = created.guid.GetCounter();
        Playerbot::Services::Owners().SetOwner(id, sess->GetAccountId(), me->GetGUID().GetCounter());
        Playerbot::Services::Population().QueueCompanionSetup(id, me->GetGUID().GetCounter(), uint8(role));
        auto const login = Playerbot::Services::SessionMgr().LoginBot(created.guid);
        if (!login.ok)
        {
            outMsg = Trinity::StringFormat("Created {} but the login was refused ({}). Try `.bot call {}`.", pick.name, login.reason, pick.name);
            return false;
        }
        outMsg = pick.name + " is on the way.";
        return true;
    }

    // The caller's companions (bots owned by their account) matching `who`: a name, or "all".
    static std::vector<Playerbot::BotId> CompanionsMatching(WorldSession const* sess, std::string const& who)
    {
        std::vector<Playerbot::BotId> out;
        std::string const want = ToLowerAscii(who);
        for (Playerbot::BotId id : Playerbot::Services::Owners().BotsOwnedBy(sess->GetAccountId()))
        {
            if (!Playerbot::Services::Lifecycle().is_bot(id))
                continue;
            std::string name;
            if (!sCharacterCache->GetCharacterNameByGuid(ObjectGuid::Create<HighGuid::Player>(id), name))
                continue;
            if (want == "all" || ToLowerAscii(name) == want)
                out.push_back(id);
        }
        return out;
    }

    // The caller's saved roster (`.bot save`), plus every companion of theirs in their group right now: a raid built
    // with `.bot add` and never saved is still the raid the owner calls (40 grouped, 6 saved: only 6 came, 2026-10-06).
    static std::vector<Playerbot::BotId> SavedCompanions(WorldSession const* sess)
    {
        std::vector<Playerbot::BotId> out;
        Player const* me = sess->GetPlayer();
        Group const* group = me ? me->GetGroup() : nullptr;
        for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
            if (Playerbot::Services::Owners().SavedBy(id, me ? me->GetGUID().GetCounter() : 0) ||
                (group && group->IsMember(ObjectGuid::Create<HighGuid::Player>(id))))
                out.push_back(id);
        return out;
    }

    static std::string CompanionName(Playerbot::BotId id)
    {
        std::string name;
        sCharacterCache->GetCharacterNameByGuid(ObjectGuid::Create<HighGuid::Player>(id), name);
        return name;
    }

    // .bot call <name|all>: log your companions in (or fetch the ones already online), level, gear and group them.
    // "all" is your saved roster (`.bot save`) and the companions in your group, not every companion you ever made.
    static bool HandleBotCall(ChatHandler* handler, std::string const& who)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        return CallCompanions(handler, who, false);
    }

    // groupOnly (`.bot summon all`, Karrz 2026-10-08: in a 2-person party it brought the whole old raid back online): "all" is only the
    // companions in the caller's group right now, wherever they are. It never logs anyone in and never adds anyone.
    static bool CallCompanions(ChatHandler* handler, std::string const& who, bool groupOnly)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return Refuse(handler); }
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me)
        { handler->SendSysMessage("`.bot call` is in-game only."); return Refuse(handler); }
        bool const roster = ToLowerAscii(who) == "all";
        std::vector<Playerbot::BotId> ids;
        if (roster && groupOnly)
        {
            // The companions in the group (one logged out is logged in again: a raid member that is offline is a missing one) and the
            // companions already online that are not in it yet: they are put in the group and brought, as `.bot call` does. Saved
            // companions that are offline and not grouped stay where they are (the 2-person-party case above).
            Group const* grp = me->GetGroup();
            uint32 grouped = 0, offlineGrouped = 0, outside = 0;
            for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
            {
                ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
                bool const online = ObjectAccessor::FindConnectedPlayer(guid) != nullptr;
                bool const inGroup = grp && grp->IsMember(guid);
                if (!inGroup && !online)
                    continue;
                ids.push_back(id);
                if (inGroup) { ++grouped; offlineGrouped += online ? 0u : 1u; }
                else ++outside;
            }
            TC_LOG_INFO("playerbot.v2", "[summon] {} .bot summon all on map {}: {} companions in the group ({} logged out), {} online outside the group",
                        me->GetName(), me->GetMapId(), grouped, offlineGrouped, outside);
            if (ids.empty())
            { handler->SendSysMessage("None of your bots are in your group or online. `.bot summon <name>` brings one, `.bot call all` brings back your saved raid."); return Refuse(handler); }
        }
        else
            ids = roster ? SavedCompanions(sess) : CompanionsMatching(sess, who);
        // `.bot call all` / `.bot summon saved` with a saved roster brings exactly that roster. A companion that is only in the group (a crafting
        // bot invited earlier) is taken out of it: it held a place and the last saved bot was refused at 40 (2026-10-11).
        if (roster && !groupOnly)
        {
            std::vector<Playerbot::BotId> savedOnly;
            for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
                if (Playerbot::Services::Owners().SavedBy(id, me->GetGUID().GetCounter()))
                    savedOnly.push_back(id);
            if (!savedOnly.empty())
            {
                Group* grp = me->GetGroup();
                uint32 removed = 0;
                for (Playerbot::BotId id : ids)
                    if (!Playerbot::Services::Owners().SavedBy(id, me->GetGUID().GetCounter()))
                    {
                        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
                        if (grp && grp->IsMember(guid))
                        {
                            grp->RemoveMember(guid);
                            Playerbot::Services::SessionMgr().LogoutBot(guid);
                            TC_LOG_INFO("playerbot.v2", "[roster_call] {} is not on the saved roster: taken out of the group", CompanionName(id));
                            ++removed;
                        }
                    }
                TC_LOG_INFO("playerbot.v2", "[roster_call] {} calls the saved roster: {} saved, {} unsaved taken out, group now {}", me->GetName(),
                            uint32(savedOnly.size()), removed, me->GetGroup() ? me->GetGroup()->GetMembersCount() : 1u);
                ids = std::move(savedOnly);
            }
        }
        if (ids.empty() && roster)
        { handler->SendSysMessage("No saved or grouped companions. `.bot save all` saves the ones with you, `.bot call <name>` calls one."); return Refuse(handler); }
        if (ids.empty())
        { handler->PSendSysMessage("No companion of yours called '%s'. `.bot list` shows them.", who.c_str()); return Refuse(handler); }
        uint32 called = 0, refused = 0;
        // Open world: at most 5 of your companions out at a time. Inside an instance as many as the map holds, and the
        // group never grows past it. The rest stay where they are.
        std::string capName;
        uint32 const cap = Playerbot::V2::Fleet::InstanceGroupCapFor(me, &capName);
        bool const inInstance = !capName.empty();
        Group const* myGroup = me->GetGroup();
        uint32 members = myGroup ? myGroup->GetMembersCount() : 1;
        uint32 out = cap ? Playerbot::V2::Fleet::CompanionsOut(me) : 0;
        for (Playerbot::BotId id : ids)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
            if (cap && !ObjectAccessor::FindConnectedPlayer(guid))
            {
                if (!inInstance && out >= cap) { ++refused; continue; }
                ++out;
            }
            if (cap && inInstance && !(myGroup && myGroup->IsMember(guid)))
            {
                if (members >= cap) { ++refused; continue; }
                ++members;
            }
            Playerbot::Services::Population().QueueCompanionSetup(id, me->GetGUID().GetCounter(), uint8(Playerbot::Role::Unknown));
            if (!ObjectAccessor::FindConnectedPlayer(guid) && !Playerbot::Services::SessionMgr().LoginBot(guid).ok)
                continue;
            ++called;
        }
        handler->PSendSysMessage("Calling %u companion(s).", called);
        if (refused)
            handler->PSendSysMessage("%s, %u companion(s) not called.",
                                     Playerbot::V2::Fleet::InstanceGroupFullText(capName, inInstance ? members : out, cap).c_str(), refused);
        return true;
    }

    // `.bot summon <all|saved|bot name>`: the every-player form of `.bot call`, for the caller's own bots only (real players are
    // summoned with the GM `.summon`, which stays GM-only).
    static bool HandleBotSummon(ChatHandler* handler, Tail args)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        WorldSession* sess = handler ? handler->GetSession() : nullptr;
        if (!sess || !sess->GetPlayer())
            return false;
        std::string who(args);
        while (!who.empty() && (who.back() == ' ' || who.back() == '\t')) who.pop_back();
        while (!who.empty() && (who.front() == ' ' || who.front() == '\t')) who.erase(who.begin());
        bool const init = Playerbot::Services::Initialized();
        // `saved` = the saved roster (`.bot save`) plus the grouped bots, logged in if need be: the addon's "Call saved" button.
        // `.bot call all` is the same.
        if (init && ToLowerAscii(who) == "saved")
        {
            CallCompanions(handler, "all", /*groupOnly*/ false);
            return true;
        }
        if (init && !who.empty() && (ToLowerAscii(who) == "all" || !CompanionsMatching(sess, who).empty()))
        {
            CallCompanions(handler, who, /*groupOnly*/ true);
            return true;
        }
        handler->SendSysMessage("You can only summon your own bots: `.bot summon all` (your group), `.bot summon saved` or `.bot summon <bot name>`.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // .learn all mylevel [player]: what a visit to the class trainers, the class quests and the weapon masters would have
    // taught at the player's current level, for free and without the NPCs (the same code the owner companions are levelled with).
    // No spell above the level, no talents.
    static bool HandleLearnAllMyLevelCommand(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        if (!player)
            player = PlayerIdentifier::FromTargetOrSelf(handler);
        if (!player || !player->IsConnected())
            return false;

        Player* target = player->GetConnectedPlayer();
        ::Playerbot::API api(target);
        uint32 const quest = api.learn_class_quest_spells_free();
        uint32 const trainer = api.learn_trainer_spells_free();
        uint32 const weapons = api.learn_weapon_skills_free();

        handler->PSendSysMessage("%s: level %u, learned %u class spells, %u class quest spells, %u weapon skills.",
            handler->GetNameLink(target).c_str(), uint32(target->GetLevel()), trainer, quest, weapons);
        return true;
    }

    // .bot itemstats: for the selected player (else yourself) the stamina each worn item really gives on this server
    // (Item::GetItemStatValue: budget share x random property points at the item level the wearer sees), next to the
    // character's total stamina, base health and max health. Compare the per-item stamina with the vanilla item_template.
    static bool HandleBotItemStats(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        { handler->SendSysMessage("Select a player or run it as a player."); return false; }
        float total = 0.0f;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item const* it = target->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!it)
                continue;
            float sta = 0.0f;
            for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                if (it->GetItemStatType(i) == ITEM_MOD_STAMINA)
                    sta += it->GetItemStatValue(i, target);
            total += sta;
            handler->PSendSysMessage("slot %u item %u ilvl %u stamina %.1f", uint32(slot), it->GetEntry(), it->GetItemLevel(target), sta);
        }
        handler->PSendSysMessage("%s: item stamina %.1f, total stamina %.1f, create health %u, max health %u", target->GetName().c_str(), total,
            target->GetStat(STAT_STAMINA), target->GetCreateHealth(), target->GetMaxHealth());
        return true;
    }

    // .bot gear [match|best|t1|t2|t3]: how the gear of the companions you own is picked (bare = show it). The same as telling a bot
    // "gear t3". Tier sets are worn at the level cap only; the server may disallow modes (Playerbot.OwnedGear.AllowedModes).
    static bool HandleBotGear(ChatHandler* handler, Optional<std::string> mode_arg)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess)
        { handler->SendSysMessage("`.bot gear` is in-game only."); return false; }
        using Playerbot::V2::Gear::Owned::GearMode;
        uint32 const account = sess->GetAccountId();
        if (!mode_arg)
        {
            handler->PSendSysMessage("Companion gear mode: %s (match, best, bis, t1, t2, t3).",
                Playerbot::V2::Gear::Owned::ModeName(GearMode(Playerbot::Services::Owners().OwnerGearMode(account))));
            return true;
        }
        GearMode mode;
        if (!Playerbot::V2::Gear::Owned::ParseGearMode(ToLowerAscii(*mode_arg), mode))
        { handler->SendSysMessage("Modes: match, best, bis, t1, t2, t3."); return false; }
        std::string reply;
        bool const ok = Playerbot::Services::Population().SetOwnerGearMode(account, uint8(mode), reply);
        handler->SendSysMessage(reply.c_str());
        return ok;
    }

    // .bot regear <name|all>: re-roll your online companions' equipment with the per-bot varied generator, so a
    // companion made before gear variety stops looking like every other bot of its class. Better worn pieces (higher
    // quality, or well above the pick) stay; replaced ones go to the bags. "all" is the same set as `.bot call all`.
    // Offline companions are skipped: `.bot call` them first.
    static bool HandleBotRegear(ChatHandler* handler, Optional<std::string> who_arg)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        std::string const who = who_arg ? *who_arg : std::string("all");   // bare `.bot regear` = every companion
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me)
        { handler->SendSysMessage("`.bot regear` is in-game only."); return false; }
        bool const roster = ToLowerAscii(who) == "all";
        auto const ids = roster ? SavedCompanions(sess) : CompanionsMatching(sess, who);
        if (ids.empty() && roster)
        { handler->SendSysMessage("No saved or grouped companions. `.bot regear <name>` regears one."); return false; }
        if (ids.empty())
        { handler->PSendSysMessage("No companion of yours called '%s'. `.bot list` shows them.", who.c_str()); return false; }
        uint32 regeared = 0;
        for (Playerbot::BotId id : ids)
        {
            std::string const name = CompanionName(id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
            if (!bot || !bot->IsInWorld())
            {
                handler->PSendSysMessage("%s is offline: `.bot call %s` first.", name.c_str(), name.c_str());
                continue;
            }
            uint32 changed = 0;
            using Reroll = Playerbot::V2::Fleet::BotPopulationManager::RerollResult;
            switch (Playerbot::Services::Population().RerollCompanionGear(bot, changed))
            {
                case Reroll::Done:
                    Playerbot::Services::Owners().MarkGearVaried(id);
                    ++regeared;
                    handler->PSendSysMessage("%s: %u slot(s) regeared.", name.c_str(), changed);
                    break;
                case Reroll::NotNow:
                    handler->PSendSysMessage("%s is in combat or dead, try again later.", name.c_str());
                    break;
                default:
                    handler->PSendSysMessage("%s is a player's own character; its gear is left alone.", name.c_str());
                    break;
            }
        }
        handler->PSendSysMessage("Regeared %u companion(s).", regeared);
        return true;
    }

    // .bot dismiss <name|all>: take your companions out of your group and log them out. They keep their gear and can be
    // called again. Leaving the group matters: a bot still listed in your group is logged back in when you log in
    // (PlayerbotV2 [GroupRelogin]), so a dismissed bot that only logged out came back at every login.
    // A summoned alt (`.playerbot summon`) is dismissed back to an ordinary character: unmarked and unbound, so the player logs in on
    // it themselves without the bot logic attaching to it (Module::OnPlayerLogin drives every marked character).
    static bool HandleBotDismiss(ChatHandler* handler, Tail args)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); handler->SetSentErrorMessage(true); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess)
            return false;
        Player* me = sess->GetPlayer();
        std::string const typed = Playerbot::V2::TypedName::Trim(std::string_view(args));
        if (typed.empty())
        {
            handler->SendSysMessage(".bot dismiss <name|all>: logs out one of your bots or summoned alts and takes it out of your group.");
            handler->SendSysMessage("Example: .bot dismiss Myalt, or .bot dismiss all");
            handler->SetSentErrorMessage(true);
            return false;
        }
        bool const all = ToLowerAscii(typed) == "all";
        std::string const name = all ? std::string("all") : TypedCharacterName(typed);
        std::vector<Playerbot::BotId> ids = name.empty() ? std::vector<Playerbot::BotId>{} : CompanionsMatching(sess, name);
        // An alt of the caller's own account that is in world as a bot is theirs to dismiss even when its owner binding is gone.
        if (ids.empty() && !all && !name.empty())
        {
            ObjectGuid const g = sCharacterCache->GetCharacterGuidByName(name);
            if (!g.IsEmpty() && sCharacterCache->GetCharacterAccountIdByGuid(g) == sess->GetAccountId() &&
                Playerbot::Services::SessionMgr().IsHeadless(g))
                ids.push_back(g.GetCounter());
        }
        if (ids.empty())
        {
            TC_LOG_INFO("playerbot.v2", "[dismiss] {}: nothing matched '{}' (looked up as '{}')", me ? me->GetName() : "?", typed, name);
            handler->PSendSysMessage("No bot or summoned alt of yours called '%s'. `.bot list` shows your bots.", typed.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        uint32 dismissed = 0;
        for (Playerbot::BotId id : ids)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
            // ike3: the companion says goodbye to its owner. Queued as a whisper intent; the logout
            // flushes it (Module::OnPlayerLogout) while the bot is still in the world.
            if (Player* bot = ObjectAccessor::FindConnectedPlayer(guid))
                Playerbot::BotChatter::SayGoodbye(bot, me);
            bool left_group = false;
            if (Group* group = me ? me->GetGroup() : nullptr; group && group->IsMember(guid))
                left_group = group->RemoveMember(guid);
            bool const logged_out = Playerbot::Services::SessionMgr().LogoutBot(guid);
            bool const own_alt = sCharacterCache->GetCharacterAccountIdByGuid(guid) == sess->GetAccountId();
            if (own_alt)
            {
                Playerbot::Services::Lifecycle().unmark_as_bot(id);
                Playerbot::Services::Owners().ClearOwner(id);
            }
            TC_LOG_INFO("playerbot.v2", "[dismiss] {} <- {}: left_group={} logged_out={} own_alt={}", CompanionName(id),
                        me ? me->GetName() : "?", left_group, logged_out, own_alt);
            if (logged_out || left_group)
                ++dismissed;
        }
        handler->PSendSysMessage("Dismissed %u companion(s).", dismissed);
        return true;
    }

    // .bot save <name|all>: put a companion, or every companion with you (online, in your group), on your roster.
    static bool HandleBotSave(ChatHandler* handler, std::string const& who)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        Player* me = sess ? sess->GetPlayer() : nullptr;
        if (!me)
            return false;
        bool const with_me = ToLowerAscii(who) == "all";
        std::string names;
        uint32 saved = 0;
        // Save group REPLACES the saved roster (playtest, 2026-10-11: after a second save the call said 78 saved where the first said 39: the
        // bots of the earlier save stayed saved next to the new ones). A companion saved earlier that is not in the group now is unsaved.
        if (with_me)
        {
            Group const* grp = me->GetGroup();
            uint32 dropped = 0, with_group = 0;
            for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
                if (grp && grp->IsMember(ObjectGuid::Create<HighGuid::Player>(id)) && ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id)))
                    ++with_group;
            // A group with none of the owner's bots in it saves nothing and must not wipe the roster.
            for (Playerbot::BotId id : CompanionsMatching(sess, "all"))
                if (with_group && Playerbot::Services::Owners().SavedBy(id, me->GetGUID().GetCounter()) && !(grp && grp->IsMember(ObjectGuid::Create<HighGuid::Player>(id))))
                {
                    Playerbot::Services::Owners().SetSaved(id, false);
                    TC_LOG_INFO("playerbot.v2", "[roster_save] {} was saved earlier and is not in the group: unsaved", CompanionName(id));
                    ++dropped;
                }
            if (dropped)
                TC_LOG_INFO("playerbot.v2", "[roster_save] Save group replaced the roster: {} earlier bot(s) unsaved", dropped);
        }
        for (Playerbot::BotId id : CompanionsMatching(sess, who))
        {
            if (with_me)
            {
                ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
                Group const* group = me->GetGroup();
                if (!ObjectAccessor::FindConnectedPlayer(guid) || !group || !group->IsMember(guid))
                    continue;
            }
            Playerbot::Services::Owners().SetSaved(id, true, me->GetGUID().GetCounter());
            // Its role is stored with it (a bot nobody ever gave a role would be re-read from its talents at the next call).
            if (Player* online = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id)))
            {
                Playerbot::Role const r = Playerbot::V2::Fleet::BotPopulationManager::StoreCurrentRole(online);
                TC_LOG_INFO("playerbot.v2", "[roster_save] {} role {}", CompanionName(id),
                            r == Playerbot::Role::Tank ? "tank" : r == Playerbot::Role::Healer ? "healer" : "dps");
            }
            // The raid sub-group it stands in is stored with it: `.bot call all` and a relog put it back there.
            if (Group const* grp = me->GetGroup(); grp && grp->isRaidGroup() && grp->IsMember(ObjectGuid::Create<HighGuid::Player>(id)))
            {
                uint8 const sub = grp->GetMemberGroup(ObjectGuid::Create<HighGuid::Player>(id));
                Playerbot::Services::Owners().SetStoredSubgroup(id, sub);
                uint16 pos = 0;
                for (Group::MemberSlot const& slot : grp->GetMemberSlots())
                {
                    if (slot.guid.GetCounter() == id)
                        break;
                    ++pos;
                }
                Playerbot::Services::Owners().SetStoredOrder(id, pos);
                TC_LOG_INFO("playerbot.v2", "[roster_save] {} saved in raid group {}", CompanionName(id), uint32(sub) + 1u);
            }
            else
                TC_LOG_INFO("playerbot.v2", "[roster_save] {} saved (not in a raid, no group stored)", CompanionName(id));
            names += (names.empty() ? "" : ", ") + CompanionName(id);
            ++saved;
        }
        if (!saved)
        {
            handler->SendSysMessage(with_me ? "None of your companions is with you: `.bot call <name>` first."
                                            : "No companion of yours by that name. `.bot list` shows them.");
            return false;
        }
        handler->PSendSysMessage("Saved %u companion(s): %s. `.bot call all` calls the saved ones.", saved, names.c_str());
        return true;
    }

    // .bot unsave <name|all>: take a companion, or all of them, off your roster. They stay yours; `.bot call <name>` still works.
    static bool HandleBotUnsave(ChatHandler* handler, std::string const& who)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess)
            return false;
        uint32 removed = 0;
        for (Playerbot::BotId id : CompanionsMatching(sess, who))
            if (Playerbot::Services::Owners().SavedBy(id, sess->GetPlayer() ? sess->GetPlayer()->GetGUID().GetCounter() : 0))
            {
                Playerbot::Services::Owners().SetSaved(id, false);
                ++removed;
            }
        handler->PSendSysMessage("Took %u companion(s) off your saved list.", removed);
        return true;
    }

    // .bot delete <name>: get rid of a companion for good (its character is deleted once it has logged out).
    static bool HandleBotDelete(ChatHandler* handler, std::string const& who)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess)
            return false;
        if (ToLowerAscii(who) == "all")
        { handler->SendSysMessage("`.bot delete` takes one name at a time."); return false; }
        auto const ids = CompanionsMatching(sess, who);
        if (ids.empty())
        { handler->PSendSysMessage("No companion of yours called '%s'. `.bot list` shows them.", who.c_str()); return false; }
        Player* me = sess->GetPlayer();
        for (Playerbot::BotId id : ids)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
            // A summoned alt is owned like a companion, but it is the player's own character: never deleted here.
            if (!Playerbot::V2::Fleet::MayDeleteCompanion(
                    Playerbot::Services::Accounts().is_pool_account(sCharacterCache->GetCharacterAccountIdByGuid(guid))))
            {
                handler->SendSysMessage(Playerbot::V2::Fleet::CompanionDeleteRefusal(CompanionName(id)).c_str());
                handler->SetSentErrorMessage(true);
                return false;
            }
            if (Group* group = me ? me->GetGroup() : nullptr; group && group->IsMember(guid))
                group->RemoveMember(guid);
            Playerbot::Services::SessionMgr().LogoutBot(guid);
            Playerbot::Services::Population().QueueCompanionDelete(id);
            handler->PSendSysMessage("%s is leaving and will be deleted.", CompanionName(id).c_str());
        }
        return true;
    }

    // .bot list: your companions, online or not.
    // `.bot world`: status of the proximity spawner (Playerbot.World.*).
    static bool HandleBotWorld(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        handler->SendSysMessage(Playerbot::Services::Population().WorldReport().c_str());
        return true;
    }

    // `.bot population [off|low|medium|high|file]`: the world bots of the whole realm, right away, until the next restart. No word: what
    // it is now. `file` hands the numbers back to the config files (Bots.Size and the other Bots.* lines, Playerbot.World.Enable).
    static bool HandleBotPopulation(ChatHandler* handler, Optional<std::string> word)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        auto& pop = Playerbot::Services::Population();
        if (word && !word->empty())
        {
            std::string const w = ToLowerAscii(*word);
            if (w == "file" || w == "config" || w == "default")
                pop.SetWorldPopulation(-1);
            else if (std::optional<Playerbot::V2::Fleet::PopLevel> const lv = Playerbot::V2::Fleet::ParsePopLevel(w))
                pop.SetWorldPopulation(int(*lv));
            else
            {
                handler->SendSysMessage("Usage: .bot population off|low|medium|high|file");
                handler->SetSentErrorMessage(true);
                return false;
            }
            static char const* const kSaid[] = { "World bots: Off until the next restart. The world bots online log out now.",
                                                 "World bots: Low. Applies now, until the next restart (then the launcher's Bots setting decides).",
                                                 "World bots: Medium. Applies now, until the next restart (then the launcher's Bots setting decides).",
                                                 "World bots: High. Applies now, until the next restart (then the launcher's Bots setting decides)." };
            int const now = pop.WorldPopulation();
            handler->SendSysMessage(now >= 0 && now <= 3 ? kSaid[now] : "World bots: back to the config files (applies now).");
            return true;
        }
        handler->SendSysMessage(pop.WorldPopulationLine().c_str());
        return true;
    }

    // `.bot bgstress <bg_type> <cycles> [hold_sec]`: repeated battleground fill / release cycles (see BotPopulationManager).
    static bool HandleBotBgStress(ChatHandler* handler, Optional<uint16> bgType, Optional<uint32> cycles, Optional<uint32> holdSec)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!bgType)
        { handler->SendSysMessage(Playerbot::Services::Population().BgStressReport().c_str()); return true; }
        handler->SendSysMessage(Playerbot::Services::Population().StartBgStress(*bgType, cycles.value_or(3), holdSec.value_or(60)).c_str());
        return true;
    }

    // `.bot bgsoak <bg_type> [max_minutes]`: one bot-only battleground played to its end (see BotPopulationManager::StartBgSoak).
    static bool HandleBotBgSoak(ChatHandler* handler, Optional<uint16> bgType, Optional<uint32> maxMinutes)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!bgType)
        { handler->SendSysMessage(Playerbot::Services::Population().BgSoakReport().c_str()); return true; }
        handler->SendSysMessage(Playerbot::Services::Population().StartBgSoak(*bgType, maxMinutes.value_or(30)).c_str());
        return true;
    }

    static bool HandleBotList(ChatHandler* handler)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess)
            return false;
        auto const ids = CompanionsMatching(sess, "all");
        if (ids.empty())
        { handler->SendSysMessage("No companions yet. `.bot add <class> [tank|healer|dps]` makes one."); return true; }
        for (Playerbot::BotId id : ids)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
            CharacterCacheEntry const* info = sCharacterCache->GetCharacterCacheByGuid(guid);
            if (!info)
                continue;
            handler->PSendSysMessage("%s: level %u %s, %s%s", info->Name.c_str(), uint32(info->Level),
                                     ClassName(info->Class), ObjectAccessor::FindConnectedPlayer(guid) ? "online" : "offline",
                                     Playerbot::Services::Owners().SavedBy(id, handler->GetSession() && handler->GetSession()->GetPlayer() ? handler->GetSession()->GetPlayer()->GetGUID().GetCounter() : 0) ? ", saved" : "");
        }
        return true;
    }

    static char const* ClassName(uint8 cls)
    {
        ChrClassesEntry const* entry = sChrClassesStore.LookupEntry(cls);
        return entry ? entry->Name[LOCALE_enUS] : "?";
    }

    static bool HandleInspect(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        {
            handler->SendSysMessage("Usage: .playerbot inspect <character_name>");
            return false;
        }

        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("Player '%s' not online.", target_name.c_str());
            return false;
        }

        const Playerbot::BotId id = p->GetGUID().GetCounter();
        const std::string report = Playerbot::Diagnostics::Inspect(id);
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // .playerbot terrainprofile <x1> <y1> <z1> <x2> <y2> <z2> <botname>
    // Samples the ACTUAL walkable collision/height surface (terrain .map + VMap
    // .vmo, exactly what Map::GetHeight sees) every 0.5y along the A->B segment,
    // to answer: is a navmesh GAP a walkable-but-unmeshed surface (VMap-walk can
    // cross it) vs a genuine void (needs an off-mesh jump) vs a wall (needs
    // routing)? The bot arg supplies the Map + PhaseShift (must be on the target
    // map). Per sample: surface Z, step delta, slope, and a wall LoS-raycast
    // between consecutive surface points. Verdict classifies the whole segment.
    static bool HandleTerrainProfile(ChatHandler* handler,
        float x1, float y1, float z1, float x2, float y2, float z2,
        std::string const& botname)
    {
        Player* p = ObjectAccessor::FindConnectedPlayerByName(botname);
        if (!p)
        {
            handler->PSendSysMessage("terrainprofile: bot '%s' not online (need a bot on the target map for its Map+phase).", botname.c_str());
            return false;
        }
        Map* m = p->GetMap();
        if (!m) { handler->SendSysMessage("terrainprofile: no map."); return false; }
        PhaseShift const& ph = p->GetPhaseShift();

        const float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
        const float hdist = std::sqrt(dx * dx + dy * dy);
        if (hdist < 0.1f) { handler->SendSysMessage("terrainprofile: segment too short."); return false; }

        // 0.5y sampling (capped so the SOAP reply stays bounded). Thresholds mirror
        // API::TryTerrainWalkFallback so the verdict matches what the crawler would do.
        constexpr float kSampleStep = 0.5f;
        const int steps = std::min(160, std::max(2, int(hdist / kSampleStep)));
        constexpr float kCliff = 3.0f;     // |dz| between 0.5y samples above this = cliff/drop (== kStepClimb)
        constexpr float kSteepDeg = 50.0f; // slope above this = steep (Recast walkable ~50-60 deg)
        constexpr float kEye = 2.0f;       // raycast height above surface for wall detection

        handler->PSendSysMessage("=== terrainprofile map=%u (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) hdist=%.1fy steps=%d ===",
            m->GetId(), x1, y1, z1, x2, y2, z2, hdist, steps);

        float prevSurf = z1, prevX = x1, prevY = y1;
        bool anyVoid = false, anyWall = false, anyCliff = false;
        float maxStep = 0.f, maxSlope = 0.f, minSurf = 1e9f, maxSurf = -1e9f;
        int firstVoid = -1, firstWall = -1, firstCliff = -1;

        for (int i = 0; i <= steps; ++i)
        {
            const float t = float(i) / float(steps);
            const float px = x1 + dx * t;
            const float py = y1 + dy * t;
            // Search from ~3y above the INTERPOLATED expected level (anchors to the
            // A->B surface line; robust for near-level gaps, won't drift through a void).
            const float refZ = (z1 + dz * t) + 3.0f;
            const float surf = m->GetHeight(ph, px, py, refZ, /*vmap*/ true, 50.0f);

            if (surf <= INVALID_HEIGHT)
            {
                anyVoid = true; if (firstVoid < 0) firstVoid = i;
                handler->PSendSysMessage("[%2d] h=%4.1f (%.1f,%.1f) surf=VOID", i, t * hdist, px, py);
                prevX = px; prevY = py; continue;
            }
            if (surf < minSurf) minSurf = surf;
            if (surf > maxSurf) maxSurf = surf;

            float dstep = 0.f, slopeDeg = 0.f;
            char const* flag = "";
            if (i > 0)
            {
                dstep = surf - prevSurf;
                const float seg = std::sqrt((px - prevX) * (px - prevX) + (py - prevY) * (py - prevY));
                slopeDeg = seg > 0.01f ? (std::atan2(std::fabs(dstep), seg) * 57.2958f) : 90.f;
                if (std::fabs(dstep) > maxStep) maxStep = std::fabs(dstep);
                if (slopeDeg > maxSlope) maxSlope = slopeDeg;
                if (std::fabs(dstep) > kCliff) { anyCliff = true; if (firstCliff < 0) firstCliff = i; flag = "CLIFF"; }
                else if (slopeDeg > kSteepDeg) flag = "steep";
                // Wall: is the straight line between the two surface points blocked?
                if (!m->isInLineOfSight(ph, prevX, prevY, prevSurf + kEye, px, py, surf + kEye,
                        LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::M2))
                { anyWall = true; if (firstWall < 0) firstWall = i; flag = "WALL"; }
            }
            handler->PSendSysMessage("[%2d] h=%4.1f (%.1f,%.1f) surf=%.2f dz=%+.2f slope=%2.0f %s",
                i, t * hdist, px, py, surf, dstep, slopeDeg, flag);
            prevSurf = surf; prevX = px; prevY = py;
        }

        char const* verdict;
        if (anyVoid)        verdict = "VOID (surface gap -> true void, needs off-mesh JUMP)";
        else if (anyWall)   verdict = "WALL (collision blocks -> needs ROUTING, VMap-walk cannot cross)";
        else if (anyCliff)  verdict = "CLIFF (>3y step -> drop/wall face, not a flat gap)";
        else if (maxSlope > kSteepDeg) verdict = "STEEP (surface present but slope >50deg -> borderline)";
        else                verdict = "CONTINUOUS WALKABLE (surface, no wall/void, gentle slope) -> VMap-walk VIABLE";
        handler->PSendSysMessage("=== VERDICT: %s ===", verdict);
        handler->PSendSysMessage("    surfZ=[%.2f..%.2f] maxStep=%.2f maxSlope=%.0f void@%d wall@%d cliff@%d",
            minSurf, maxSurf, maxStep, maxSlope, firstVoid, firstWall, firstCliff);
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::SystemStatus();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleActivity(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetActivity();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleLevels(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetLevels();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleMaps(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetMaps();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleQuests(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetQuests();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleCrafting(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetCrafting();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // .playerbot flips: bots that stop and start because two rules take turns (Diagnostics::FleetFlips)
    static bool HandleFlips(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetFlips();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleStuck(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetStuck();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBg(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetBg();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleMark(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::Services::Lifecycle().mark_as_bot(id);
        // Bind ownership to the GM running the command when invoked
        // in-game (not from console). The whisper-command authority
        // model uses owner_account_id to decide who can issue squad
        // commands, so binding at mark-time gives the GM immediate
        // control without a separate adopt step. Console invocation
        // (no Player session) leaves the binding empty â useful for
        // batch-creating bots that an admin will assign later.
        if (Player const* gm = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr)
        {
            Playerbot::Services::Owners().SetOwner(
                id,
                handler->GetSession()->GetAccountId(),
                /*player_guid=*/ 0u);   // any char on the GM's account
            handler->PSendSysMessage("Owner: account %u (any char on it).",
                                     handler->GetSession()->GetAccountId());
            (void)gm;
        }
        // Already-online characters get the registry entry without a re-login,
        // so the bot ticks can start at the next world update. Offline characters
        // get the marker only â the next time they log in through `.playerbot login`, OnPlayerLogin
        // sees is_bot()==true and attaches the bot logic. A login with the game
        // client releases the mark instead (Session/HumanLogin.h).
        if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(target_name))
        {
            if (!Playerbot::Services::Registry().has(id))
            {
                Playerbot::BotPersonality personality =
                    Playerbot::Services::Config().random_personality()
                        ? Playerbot::RandomPersonality(Playerbot::SeedForBot(id))
                        : Playerbot::DefaultPersonality();
                Playerbot::Services::Registry().register_bot(
                    id, personality,
                    Playerbot::BotRng{Playerbot::SeedForBot(id)});
                Playerbot::Services::Scheduler().register_bot(
                    id, Playerbot::ActivityTier::Idle);
            }
            handler->PSendSysMessage("Marked %s (online) as a V2 bot â bot logic active.",
                                     p->GetName().c_str());
        }
        else
        {
            handler->PSendSysMessage("Marked '%s' as a V2 bot. Bot logic activates on next login.",
                                     target_name.c_str());
        }
        return true;
    }

    static bool HandleUnmark(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::Services::Lifecycle().unmark_as_bot(id);
        // If the character is online and currently registered, drop the
        // registry entry too so the bot logic stops driving them this session.
        if (Playerbot::Services::Registry().has(id))
        {
            Playerbot::Services::Scheduler().unregister_bot(id);
            Playerbot::Services::Snapshots().remove(id);
            Playerbot::Services::Registry().unregister_bot(id);
        }
        handler->PSendSysMessage("Unmarked '%s'. Bot logic detached.", target_name.c_str());
        // Owner binding becomes meaningless without the bot â clear it so
        // a future re-mark of the same character starts clean.
        Playerbot::Services::Owners().ClearOwner(id);
        return true;
    }

    // Adopt / disown / owner â owner binding management. Decoupled from
    // mark/unmark so an admin can transfer ownership (e.g. between two
    // accounts on the same realm) without touching the bot's identity.

    static bool HandleAdopt(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        { handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
          return false; }
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        { handler->PSendSysMessage("'%s' is not a marked bot. Use .playerbot mark first.",
                                   target_name.c_str());
          return false; }
        // Console invocation has no session â can't determine an owner.
        // Tell the operator to adopt from in-game (or use a SQL UPDATE
        // for batch ownership transfer).
        if (!handler->GetSession())
        { handler->SendSysMessage("Adopt requires an in-game session (console can't own bots).");
          return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        Playerbot::Services::Owners().SetOwner(id, account, /*player_guid=*/ 0u);
        handler->PSendSysMessage("Owner of '%s' set to account %u.",
                                 target_name.c_str(), account);
        return true;
    }

    static bool HandleDisown(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        { handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
          return false; }
        Playerbot::Services::Owners().ClearOwner(id);
        handler->PSendSysMessage("Owner of '%s' cleared.", target_name.c_str());
        return true;
    }

    static bool HandleOwner(ChatHandler* handler, Optional<std::string> player_arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        // Default target: the GM running the command.
        uint32 account = 0;
        std::string label = "<unknown>";
        if (player_arg)
        {
            // Resolve the named player â account_id via DB. Online lookup
            // first (fast); fall back to characters â account.
            if (Player const* p = ObjectAccessor::FindConnectedPlayerByName(*player_arg))
            {
                account = p->GetSession()->GetAccountId();
                label = p->GetName();
            }
            else
            {
                CharacterCacheEntry const* cce =
                    sCharacterCache->GetCharacterCacheByName(*player_arg);
                if (!cce)
                { handler->PSendSysMessage("No character named '%s' exists.",
                                           player_arg->c_str());
                  return false; }
                account = cce->AccountId;
                label = cce->Name;
            }
        }
        else
        {
            if (!handler->GetSession())
            { handler->SendSysMessage("Specify a player name when using from console.");
              return false; }
            account = handler->GetSession()->GetAccountId();
            label = handler->GetSession()->GetPlayer()
                  ? handler->GetSession()->GetPlayer()->GetName()
                  : "console-self";
        }

        std::vector<Playerbot::BotId> const owned =
            Playerbot::Services::Owners().BotsOwnedBy(account);
        if (owned.empty())
        { handler->PSendSysMessage("'%s' (account %u) owns 0 bots.",
                                   label.c_str(), account);
          return true; }
        handler->PSendSysMessage("'%s' (account %u) owns %zu bot(s):",
                                 label.c_str(), account, owned.size());
        // Print up to 20 names to keep it readable; full list belongs in
        // a future paginated `.playerbot squad <player>` view.
        size_t shown = 0;
        for (Playerbot::BotId id : owned)
        {
            if (shown++ >= 20) { handler->PSendSysMessage("  ... (%zu more)",
                                                          owned.size() - 20);
                                  break; }
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            CharacterCacheEntry const* cce =
                sCharacterCache->GetCharacterCacheByGuid(g);
            handler->PSendSysMessage("  %s (id=%llu)",
                cce ? cce->Name.c_str() : "<unknown>",
                static_cast<unsigned long long>(id));
        }
        return true;
    }

    static bool HandleSquadSave(ChatHandler* handler, std::string const& preset_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!handler->GetSession())
        { handler->SendSysMessage("Squad save: requires an in-game session.");
          return false; }
        if (preset_name.empty())
        { handler->SendSysMessage("Squad save: usage <preset_name>"); return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        const size_t n = Playerbot::Services::Owners().SaveSquadPreset(account, preset_name);
        if (n == 0)
        { handler->PSendSysMessage("Squad save: 0 bots â nothing to save.");
          return false; }
        handler->PSendSysMessage("Squad save: saved %zu bot(s) under preset '%s'.",
                                 n, preset_name.c_str());
        return true;
    }

    static bool HandleSquadLoad(ChatHandler* handler, std::string const& preset_name)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        if (!handler->GetSession())
        { handler->SendSysMessage("Squad load: requires an in-game session.");
          return false; }
        if (preset_name.empty())
        { handler->SendSysMessage("Squad load: usage <preset_name>"); return false; }
        const uint32 account = handler->GetSession()->GetAccountId();
        // 1) Write the preset into each owned bot's character row.
        const size_t n = Playerbot::Services::Owners().LoadSquadPreset(account, preset_name);
        if (n == 0)
        { handler->PSendSysMessage("Squad load: preset '%s' not found or applied to 0 bots.",
                                   preset_name.c_str());
          return false; }
        // 2) Push the now-current persisted state to every live bot's
        //    in-memory BotAI so the formation re-snaps without a relog.
        //    Re-emits FollowIntent if the bot is in active Follow mode
        //    so the new offset takes effect immediately.
        size_t live_applied = 0;
        for (Playerbot::BotId id : Playerbot::Services::Owners().BotsOwnedBy(account))
        {
            Playerbot::BotAI* ai = Playerbot::Services::Registry().ai(id);
            if (!ai) continue;
            const auto s = Playerbot::Services::Owners().LoadSquadState(id);
            ai->set_formation_type(static_cast<Playerbot::FormationType>(s.formation_type));
            ai->set_formation_slot(s.formation_slot);
            ai->set_follow_distance(s.follow_distance);
            ai->set_verbose_logging(s.owner_verbose);
            // Re-emit follow with new offset if the bot is currently
            // tracking someone â gives instant visual feedback.
            if (ai->owner_command() == Playerbot::BotAI::OwnerCommand::Follow &&
                !ai->owner_target().IsEmpty())
            {
                Playerbot::IntentQueue* q = Playerbot::Services::Registry().intents(id);
                Playerbot::IntentId* next = Playerbot::Services::Registry().next_intent_id(id);
                if (q && next)
                {
                    Playerbot::FormationOffset const off =
                        Playerbot::ComputeFormationOffset(
                            ai->formation_type(), ai->formation_slot(),
                            ai->follow_distance() > 0.f ? ai->follow_distance() : 5.0f);
                    Playerbot::BotIntentEmitter emit(q, id, /*source*/ 0, next);
                    emit.follow(ai->owner_target(), off.distance, off.angle_radians);
                }
            }
            ++live_applied;
        }
        handler->PSendSysMessage("Squad load: %zu bot(s) updated (%zu live re-snapped).",
                                 n, live_applied);
        return true;
    }

    static bool HandleCount(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const size_t marked     = Playerbot::Services::Lifecycle().size();
        const size_t registered = Playerbot::Services::Registry().size();
        // Walk marked ids and count how many resolve to a connected Player â
        // gives the actual "in-world right now" number, distinct from
        // registered (registered counts every bot whose bot logic exists, including
        // ones whose Player* may have despawned mid-tick).
        const auto ids = Playerbot::Services::Lifecycle().snapshot_ids();
        size_t online = 0;
        for (auto id : ids)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            if (ObjectAccessor::FindConnectedPlayer(g)) ++online;
        }
        // Pool stats: how many PBV2_NNNN accounts exist and how many slots
        // they're holding. Free slots = pool_size * 10 - total_chars (ceiling
        // of growable; AcquireSlot creates a new account when all are full).
        const size_t pool_acct = Playerbot::Services::Accounts().pool_size();
        const size_t pool_chrs = Playerbot::Services::Accounts().total_chars();
        const size_t cap       = pool_acct * Playerbot::V2::BotAccountMgr::MAX_CHARS_PER_ACCOUNT;
        const size_t free_now  = cap > pool_chrs ? cap - pool_chrs : 0;

        handler->PSendSysMessage("Bots: %zu marked / %zu registered / %zu in-world.",
                                 marked, registered, online);
        handler->PSendSysMessage("Pool: %zu account(s) / %zu chars / %zu free slot(s) before next account auto-create.",
                                 pool_acct, pool_chrs, free_now);
        return true;
    }

    static bool HandleList(ChatHandler* handler, Optional<uint32> page_arg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        constexpr size_t kPageSize = 30;
        const size_t page = page_arg.value_or(0u);
        const auto ids = Playerbot::Services::Lifecycle().snapshot_ids();
        if (ids.empty())
        {
            handler->SendSysMessage("No marked bots in playerbot_v2_character.");
            return true;
        }
        // Class id â 3-letter tag for compact line formatting. Indexed by the
        // SharedDefines CLASS_* values 1..13 (rows 0/12 are empty/Monk gap).
        static char const* const kClassTag[] = {
            "---", "WAR", "PAL", "HUN", "ROG", "PRI", "DK ",
            "SHA", "MAG", "WLK", "MNK", "DRU", "DH ", "EVK"
        };
        const size_t first = page * kPageSize;
        if (first >= ids.size())
        {
            handler->PSendSysMessage("Page %zu out of range (have %zu page(s)).",
                                     page, (ids.size() + kPageSize - 1) / kPageSize);
            return true;
        }
        const size_t last = std::min(first + kPageSize, ids.size());
        handler->PSendSysMessage("Marked bots %zu-%zu of %zu (page %zu):",
                                 first + 1, last, ids.size(), page);
        for (size_t i = first; i < last; ++i)
        {
            const Playerbot::BotId id = ids[i];
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            CharacterCacheEntry const* ce = sCharacterCache->GetCharacterCacheByGuid(g);
            const bool online = ObjectAccessor::FindConnectedPlayer(g) != nullptr;
            const bool registered = Playerbot::Services::Registry().has(id);
            char const* state = online ? (registered ? "ON+OK" : "ON   ")
                                       : (registered ? "REG  " : "off  ");
            if (ce)
            {
                handler->PSendSysMessage(" #%llu  %s  L%-2u %s  %s",
                    static_cast<unsigned long long>(id),
                    state,
                    ce->Level,
                    kClassTag[ce->Class < 14 ? ce->Class : 0],
                    ce->Name.c_str());
            }
            else
            {
                // Cache miss usually means the row was deleted out from under us
                // â keep the line so the GM can spot orphans and unmark them.
                handler->PSendSysMessage(" #%llu  %s  <cache miss â orphan?>",
                    static_cast<unsigned long long>(id), state);
            }
        }
        if (last < ids.size())
            handler->PSendSysMessage("Use .playerbot list %zu for the next page.",
                                     page + 1);
        return true;
    }

    static bool HandleLogin(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        auto res = Playerbot::Services::SessionMgr().LoginBot(g);
        if (!res.ok)
        {
            handler->PSendSysMessage("Login refused: %s", res.reason.c_str());
            return false;
        }
        handler->PSendSysMessage("Spawn-login submitted for '%s' â bot enters world on next DB callback.",
                                 target_name.c_str());
        return true;
    }

    static bool HandleLogout(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (!Playerbot::Services::SessionMgr().LogoutBot(g))
        {
            handler->PSendSysMessage("No active V2 bot session for '%s'.", target_name.c_str());
            return false;
        }
        handler->PSendSysMessage("Logout requested for '%s'.", target_name.c_str());
        return true;
    }

    static bool HandleLoginAll(ChatHandler* handler, Optional<uint32> cap_arg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // Default cap: 100 simultaneous V2 bot sessions. Tunable per-call to
        // avoid flooding the character DB async pool with login holders. The
        // cap counts active sessions (in-flight + in-world); we stop submitting
        // when reached so a re-run picks up where we stopped.
        const uint32 cap = cap_arg.value_or(100u);
        auto r = Playerbot::Services::SessionMgr().LoginAll(cap);
        handler->PSendSysMessage("login_all: %u attempted / %u submitted / %u already in-world (cap %u).",
                                 r.attempted, r.succeeded, r.skipped_already_in_world, cap);
        return true;
    }

    static bool HandleLogoutAll(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const uint32 n = Playerbot::Services::SessionMgr().LogoutAll();
        handler->PSendSysMessage("logout_all: %u V2 bot session(s) kicked.", n);
        return true;
    }

    // .playerbot create [name] [race] [class] [gender]
    //
    // Creates a new bot character on a pool account ("PBV2_NNNN", up to 10
    // chars per account, auto-allocated by BotAccountMgr), marks it as V2,
    // and auto-submits a headless login.
    //
    // All args optional. Anything missing is filled by Blizzlike random
    // (BotComposition) â race/class weights from public WoW census, with
    // race/class restrictions enforced (DH=NE/BE only, Evoker=Dracthyr only).
    // Race/class accept names ("warrior","nightelf","be","dh") or numeric ids.
    static bool HandleCreate(ChatHandler* handler,
                             Optional<std::string> nameArg,
                             Optional<std::string> raceArg,
                             Optional<std::string> classArg,
                             Optional<uint32>      genderArg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }

        // ownerSession is optional. BotCharacterFactory::Create accepts a null
        // session (it spins up a temporary BotSession owned by the bot-pool
        // account to own the new character, and defaults name-validation locale
        // to enUS). This lets `.playerbot create` run from a sessionless caller
        // â the SOAP/RA remote console or the server CLI â which is required for
        // headless fleet automation. Only the name locale differs from an
        // in-game GM invocation.
        WorldSession* sess = handler->GetSession();

        // Resolve race/class/gender/name. If any are missing or unparseable,
        // fall back to a Blizzlike Roll which respects race/class restrictions.
        // Explicit args win â Roll receives them as hints so the rolled
        // fields are constrained around them (e.g., specifying race=nightelf
        // rolls a class from NE's legal subset, not a random class that
        // might be invalid for NE).
        uint8 race   = raceArg   ? ParseRace(*raceArg)   : 0;
        uint8 cls    = classArg  ? ParseClass(*classArg) : 0;
        uint8 gender = uint8(genderArg.value_or(0xFFu));    // sentinel = unset
        std::string name = nameArg ? *nameArg : std::string{};

        const bool needsRoll = (race == 0) || (cls == 0) || (gender == 0xFF) || name.empty();
        if (needsRoll)
        {
            auto picked = Playerbot::V2::BotComposition::Roll(race, cls, gender, name);
            if (picked.race == 0)
            {
                handler->SendSysMessage("BotComposition::Roll failed â possibly invalid race/class hint. See log.");
                return false;
            }
            race = picked.race; cls = picked.cls; gender = picked.gender; name = picked.name;
        }

        auto r = Playerbot::V2::BotCharacterFactory::Create(sess, name, race, cls, gender);
        if (!r.ok)
        {
            handler->PSendSysMessage("create failed: %s", r.reason.c_str());
            return false;
        }
        handler->PSendSysMessage("Created '%s' (guid %s) â race %u class %u gender %u â marked as V2 bot.",
                                 name.c_str(), r.guid.ToString().c_str(), race, cls, gender);

        // Auto-login so the GM sees the bot in-world without a second command.
        auto login = Playerbot::Services::SessionMgr().LoginBot(r.guid);
        if (!login.ok)
        {
            handler->PSendSysMessage("Spawn-login refused: %s â use `.playerbot login %s` later.",
                                     login.reason.c_str(), name.c_str());
            return true;
        }
        handler->PSendSysMessage("Spawn-login submitted â bot enters world on next DB callback.");
        return true;
    }

    // .playerbot dist
    //
    // Print the configured Blizzlike distribution weights so operators can
    // see what to expect from `.playerbot create` (no args) and `.playerbot
    // spawn`. Read-only diagnostic; does not change anything.
    static bool HandleDist(ChatHandler* handler)
    {
        const std::string desc = Playerbot::V2::BotComposition::DescribeWeights();
        for (auto const& line : Trinity::Tokenize(desc, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // .playerbot summonall
    //
    // Teleport every in-world V2 bot to the GM's current position. Skips
    // offline bots (use `.playerbot loginall` first). Skips bots in BG/arena
    // because the cross-instance teleport requires the GM-summon machinery
    // and a partial implementation here would silently break BG queues.
    static bool HandleSummonAll(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* gm = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!gm)
        {
            handler->SendSysMessage("summonall requires an in-game GM (need a Player to teleport to).");
            return false;
        }

        const uint32 dstMap = gm->GetMapId();
        const float  dx = gm->GetPositionX();
        const float  dy = gm->GetPositionY();
        const float  dz = gm->GetPositionZ();
        const float  do_ = gm->GetOrientation();

        uint32 summoned = 0, skipped = 0, offline = 0;
        for (Playerbot::BotId id : Playerbot::Services::Lifecycle().snapshot_ids())
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) { ++offline; continue; }
            if (p->GetMap() && p->GetMap()->IsBattlegroundOrArena()) { ++skipped; continue; }
            if (p->IsBeingTeleported()) { ++skipped; continue; }

            Playerbot::BotMovement::SafeTeleport(p, dstMap, dx, dy, dz, do_,
                                                 /*options*/ 0);
            ++summoned;
        }

        handler->PSendSysMessage("summonall: %u summoned / %u skipped (BG/teleporting) / %u offline.",
                                 summoned, skipped, offline);
        return true;
    }

    // .playerbot levelall <level>
    //
    // Apply .playerbot level to every in-world V2 bot. Same Player::GiveLevel
    // + InitTalentForLevel + SetXP(0) + apply_starter_talents pipeline as the
    // single-target command. Skips offline bots and bots in combat.
    static bool HandleLevelAll(ChatHandler* handler, uint32 level)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        if (level < 1 || level > maxLevel)
        {
            handler->PSendSysMessage("level must be 1..%u.", maxLevel);
            return false;
        }

        uint32 leveled = 0, in_combat = 0, offline = 0, talents_applied = 0;
        for (Playerbot::BotId id : Playerbot::Services::Lifecycle().snapshot_ids())
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) { ++offline; continue; }
            if (p->IsInCombat()) { ++in_combat; continue; }

            p->GiveLevel(uint8(level));
            p->InitTalentForLevel();
            p->SetXP(0);
            ++leveled;

            if (level >= 10)
            {
                Playerbot::API api(p);
                if (api.apply_starter_talents() == Playerbot::Result::Ok)
                    ++talents_applied;
            }
        }

        handler->PSendSysMessage("levelall: %u leveled to %u (talents applied: %u) / %u in combat / %u offline.",
                                 leveled, level, talents_applied, in_combat, offline);
        return true;
    }

    // .playerbot spawn [count]
    //
    // Batch-create N fully-random Blizzlike bots in one shot. Each gets a
    // unique generated name, race, class, gender, on a pool account from
    // BotAccountMgr. Each is auto-logged-in. Default count: 5. Hard cap: 50
    // per invocation to avoid surprise mass-spawn.
    static bool HandleSpawn(ChatHandler* handler, Optional<uint32> countArg)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // Sessionless callers (SOAP/RA console, server CLI) are allowed â the
        // factory owns each new character with a temporary bot-pool BotSession.
        // See HandleCreate for the rationale; enables headless mass-spawn.
        WorldSession* sess = handler->GetSession();

        constexpr uint32 kDefaultCount = 5;
        constexpr uint32 kHardCap      = 50;
        const uint32 requested = countArg.value_or(kDefaultCount);
        const uint32 count     = std::min(requested, kHardCap);

        uint32 created = 0, login_ok = 0, login_fail = 0;
        for (uint32 i = 0; i < count; ++i)
        {
            auto picked = Playerbot::V2::BotComposition::Roll();
            if (picked.race == 0) break;

            auto r = Playerbot::V2::BotCharacterFactory::Create(
                sess, picked.name, picked.race, picked.cls, picked.gender);
            if (!r.ok)
            {
                handler->PSendSysMessage("[%u/%u] create failed: %s", i+1, count, r.reason.c_str());
                continue;
            }
            ++created;
            auto login = Playerbot::Services::SessionMgr().LoginBot(r.guid);
            if (login.ok) ++login_ok; else ++login_fail;
        }

        handler->PSendSysMessage("spawn: created %u/%u bots; auto-login %u ok / %u failed%s.",
                                 created, count, login_ok, login_fail,
                                 (requested > kHardCap) ? " (clamped to 50)" : "");
        return true;
    }

    // .playerbot money <name> <copper>
    //
    // Set a bot's money to <copper> (1g = 10000c). Useful for testing the
    // vendor/auction/train/repair auto-rules which gate on bot.gold. Caps
    // at MAX_MONEY_AMOUNT (~10b copper, far above any realistic test value).
    static bool HandleMoney(ChatHandler* handler,
                            std::string const& target_name,
                            uint64 copper)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world.", target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot.", target_name.c_str());
            return false;
        }
        const uint64 capped = std::min<uint64>(copper, MAX_MONEY_AMOUNT);
        p->SetMoney(capped);
        handler->PSendSysMessage("'%s' money set to %llu copper (%llug %llus %lluc).",
                                 target_name.c_str(),
                                 static_cast<unsigned long long>(capped),
                                 static_cast<unsigned long long>(capped / 10000),
                                 static_cast<unsigned long long>((capped % 10000) / 100),
                                 static_cast<unsigned long long>(capped % 100));
        return true;
    }

    // .playerbot reset <name>
    //
    // Make a bot ready for the next test run: stop combat, interrupt any
    // cast, drop all auras, full HP/mana, clear cooldowns. Eliminates the
    // multi-step cycle of healing + buff-clearing + waiting for the combat
    // timer between iterative tests.
    static bool HandleReset(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world.", target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot â refusing to reset a "
                                     "non-bot character.", target_name.c_str());
            return false;
        }

        // Order matters: interrupt cast first so the spell-end packet doesn't
        // re-apply auras we're about to drop. CombatStop ends combat (clears
        // threat lists + drops PvP combat flag). RemoveAllAuras then strips
        // buffs/debuffs/HoTs/DoTs. SetFullHealth + SetPower restore resources.
        // GetSpellHistory()->ResetAllCooldowns flushes the cooldown map +
        // global cooldown â same primitive used by the /cdreset whisper.
        p->InterruptNonMeleeSpells(true);
        p->CombatStop();
        p->RemoveAllAuras();
        p->SetFullHealth();
        for (uint32 power = 0; power < MAX_POWERS; ++power)
            p->SetPower(Powers(power), p->GetMaxPower(Powers(power)));
        p->GetSpellHistory()->ResetAllCooldowns();

        handler->PSendSysMessage("Reset '%s' â combat dropped, auras cleared, HP/mana full, "
                                 "cooldowns flushed.", target_name.c_str());
        return true;
    }

    // .playerbot level <name> <level>
    //
    // Instantly level an in-world V2 bot via Player::GiveLevel. Bot must be
    // in-world (use `.playerbot login` first); level must be 1..MaxPlayerLevel.
    // GiveLevel handles stat/skill/talent/spell scaling in one shot â no
    // per-level loop needed. The packet it sends drops via BotSession::SendPacket.
    // .playerbot questrun <bot> <questId> [specId]: stage a world bot on one quest for the quest protocol (operator 2026-11-08).
    // One step per call, re-run until it says accepted: spec and talents, level up to the quest, repair, bags down to quest
    // items, gear, teleport to the quest giver's spawn, accept. The bot's own quest rules do the rest.
    static bool HandleQuestRun(ChatHandler* handler, std::string const& target_name, uint32 questId, Optional<uint32> specArg)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p || !p->IsInWorld())
        { handler->PSendSysMessage("questrun: '%s' is not in-world (.playerbot login %s first).", target_name.c_str(), target_name.c_str()); return false; }
        const BotId id = p->GetGUID().GetCounter();
        if (!Services::Lifecycle().is_bot(id))
        { handler->PSendSysMessage("questrun: '%s' is not a V2 bot.", target_name.c_str()); return false; }
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
        { handler->PSendSysMessage("questrun: no quest %u.", questId); return false; }

        auto say = [&](char const* step, std::string const& detail)
        {
            handler->PSendSysMessage("questrun %s quest %u: %s %s", p->GetName().c_str(), questId, step, detail.c_str());
            TC_LOG_INFO("playerbot.v2", "[questrun] {} quest={} step={} {}", p->GetName(), questId, step, detail);
        };

        // The quest giver's spawn: of every creature and game object that starts the quest, the first spawn on a map that is not
        // instanceable; a spawn inside an instance only when there is nothing else. One scan of the spawn tables per quest.
        struct Giver { uint32 entry = 0; bool go = false; bool open = false; WorldLocation where; };
        static std::unordered_map<uint32, Giver> s_givers;
        auto git = s_givers.find(questId);
        if (git == s_givers.end())
        {
            Giver g;
            auto consider = [&](uint32 mapId, Position const& at, uint32 entry, bool isGo)
            {
                MapEntry const* map = sMapStore.LookupEntry(mapId);
                bool const open = map && !map->Instanceable();
                if (!g.entry || (open && !g.open))
                { g.entry = entry; g.go = isGo; g.open = open; g.where = WorldLocation(mapId, at); }
            };
            std::unordered_set<uint32> creatures, objects;
            for (auto const& [entry, q] : *sObjectMgr->GetCreatureQuestRelationMapHACK())
                if (q == questId) creatures.insert(entry);
            for (auto const& [entry, q] : *sObjectMgr->GetGOQuestRelationMapHACK())
                if (q == questId) objects.insert(entry);
            if (!creatures.empty())
                for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
                    if (creatures.count(data.id)) { consider(data.mapId, data.spawnPoint, data.id, false); if (g.open) break; }
            if (!objects.empty() && !g.open)
                for (auto const& [spawnId, data] : sObjectMgr->GetAllGameObjectData())
                    if (objects.count(data.id)) { consider(data.mapId, data.spawnPoint, data.id, true); if (g.open) break; }
            git = s_givers.emplace(questId, g).first;
        }
        Giver const& giver = git->second;
        if (!giver.entry)
        { say("no_giver", "no creature or game object spawn starts this quest"); return false; }

        // Every call, so a bot that plays a long quest is never logged out by the shaper (the driver calls back while it plays).
        Services::Population().ProtectFromKick(id, 60u * 60u * 1000u);
        QuestStatus const status = p->GetQuestStatus(questId);
        if (status != QUEST_STATUS_NONE)
        { say("accepted", fmt::format("already in the log, status {}", uint32(status))); return true; }
        if (p->GetQuestRewardStatus(questId))
        { say("rewarded", "already rewarded"); return true; }
        if (p->InBattleground() || p->InArena() || p->InBattlegroundQueue())
        { say("in_battleground", "refused: leave the battleground first"); return false; }
        if (Group* g = p->GetGroup())
        { g->RemoveMember(p->GetGUID()); say("left_group", "a quest is tested alone"); return true; }

        if (!p->IsAlive())
        { p->ResurrectPlayer(1.0f); p->SpawnCorpseBones(); say("revived", ""); return true; }
        if (p->IsInCombat())
            p->CombatStop(true);

        // Talents of the asked tree: the old ones go first, the build is walked until it says Ok (one walk per call). The level
        // step below clears the mark, so new points are placed the same way.
        static std::map<BotId, uint32> s_specDone;   // bot -> spec whose build was applied
        uint32 const want = uint32(std::max<int32>(quest->GetClassicQuestLevel(), quest->GetClassicMinLevel()));
        uint32 const cap = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        bool const needsLevel = want > p->GetLevel() && want <= cap;
        if (!needsLevel && specArg && *specArg && s_specDone[id] != *specArg)
        {
            if (Classic::GetSpecOverride(p->GetGUID()) != *specArg)
            {
                if (!Classic::SetSpecOverride(p, *specArg))
                {
                    // a chain runs through quests of every class on one bot: a spec of another class is simply not applied
                    s_specDone[id] = *specArg;
                    say("spec_skipped", fmt::format("{} is not a spec of this class, talents left as they are", *specArg));
                    return true;
                }
                p->ResetTalents(true);
            }
            Result const r = API(p).apply_talent_build(0);
            if (r != Result::Ok)
            { say("spec_pending", fmt::format("override {} set, talent build result {}, run again", *specArg, int(r))); return false; }
            s_specDone[id] = *specArg;
            say("spec", fmt::format("override {} set, talents reset and the build applied", *specArg));
            return true;
        }

        if (needsLevel)
        {
            p->GiveLevel(uint8(want));
            p->InitTalentForLevel();
            p->SetXP(0);
            V2::Fleet::BotSetupPipeline::LearnLevelKit(p);
            s_specDone.erase(id);   // the new points are placed by the spec step on the next call (gated on its result)
            say("levelled", fmt::format("to {} (quest level {}, min {})", want, quest->GetClassicQuestLevel(), quest->GetClassicMinLevel()));
            return true;
        }

        // Upkeep once per bot and quest: repair, bags, gear.
        static std::set<std::pair<BotId, uint32>> s_prepared;
        if (!s_prepared.count({ id, questId }))
        {
            p->DurabilityRepairAll(false, 0.f, false);
            std::vector<std::pair<uint8, uint8>> slots;
            p->ForEachItem(ItemSearchLocation::Inventory, [&](Item* item)
            {
                ItemTemplate const* tmpl = item->GetTemplate();
                if (!item->IsBag() && tmpl && tmpl->GetClass() != ITEM_CLASS_QUEST)
                    slots.emplace_back(item->GetBagSlot(), item->GetSlot());
                return ItemSearchCallbackResult::Continue;
            });
            for (auto const& [bag, slot] : slots)
                p->DestroyItem(bag, slot, true);
            bool const regeared = Services::Population().RegearNow(p);
            s_prepared.insert({ id, questId });
            say("upkeep", fmt::format("repaired, {} bag items destroyed, regear changed items {}", slots.size(), int(regeared)));
            return true;
        }

        if (p->GetMapId() != giver.where.GetMapId() || p->GetExactDist2d(giver.where.GetPositionX(), giver.where.GetPositionY()) > 40.f)
        {
            bool const ok = BotMovement::SafeTeleport(p, giver.where.GetMapId(), giver.where.GetPositionX(), giver.where.GetPositionY(), giver.where.GetPositionZ(), 0.f, 0);
            say(ok ? "teleported" : "teleport_failed", fmt::format("to {} {} at map {} ({:.0f}, {:.0f}, {:.0f})", giver.go ? "object" : "creature", giver.entry,
                giver.where.GetMapId(), giver.where.GetPositionX(), giver.where.GetPositionY(), giver.where.GetPositionZ()));
            return ok;
        }

        // The bot's own travel runs it off the giver within seconds of the teleport (12 s between calls is 80 y of running), and
        // quest talk needs 5 y: the giver is looked for within 250 y and the quest taken without the talk range. The test is
        // about the quest, not the walk to its giver.
        WorldObject* giverObj = nullptr;
        if (giver.go)
            giverObj = p->FindNearestGameObject(giver.entry, 250.f);
        else
            giverObj = p->FindNearestCreature(giver.entry, 250.f);
        if (!giverObj)
        { say("giver_not_here", fmt::format("{} {} not within 250 y (not spawned, or the grid is still loading: run again)", giver.go ? "object" : "creature", giver.entry)); return false; }
        Result r = API(p).accept_quest(giverObj->GetGUID(), questId);
        if (r == Result::OutOfRange && p->GetQuestStatus(questId) == QUEST_STATUS_NONE && p->CanTakeQuest(quest, false) && p->CanAddQuest(quest, false))
        {
            p->AddQuestAndCheckCompletion(quest, giverObj);
            r = Result::Ok;
        }
        if (r == Result::Ok || p->GetQuestStatus(questId) != QUEST_STATUS_NONE)
        { say("accepted", fmt::format("from {}", giver.entry)); return true; }
        // which gate of CanTakeQuest refused, so the driver can act (run the previous quest first, level, skip a class quest)
        std::string why;
        if (!p->SatisfyQuestLevel(quest, false)) why += "level ";
        if (!p->SatisfyQuestClass(quest, false)) why += "class ";
        if (!p->SatisfyQuestRace(quest, false)) why += "race ";
        if (!p->SatisfyQuestSkill(quest, false)) why += "skill ";
        if (!p->SatisfyQuestReputation(quest, false)) why += "reputation ";
        if (!p->SatisfyQuestPreviousQuest(quest, false)) why += fmt::format("prev={} ", quest->GetPrevQuestId());
        if (!p->SatisfyQuestDependentQuests(quest, false)) why += "dependent ";
        if (!p->SatisfyQuestBreadcrumbQuest(quest, false) || !p->SatisfyQuestDependentBreadcrumbQuests(quest, false)) why += "breadcrumb ";
        if (!p->SatisfyQuestExclusiveGroup(quest, false)) why += "exclusive ";
        if (!p->SatisfyQuestConditions(quest, false)) why += "conditions ";
        if (!p->SatisfyQuestExpansion(quest, false)) why += "expansion ";
        if (!p->SatisfyQuestLog(false)) why += "log_full ";
        say("accept_refused", fmt::format("result {} gates: {}", int(r), why.empty() ? "none failed (see [accept_quest] reason=)" : why));
        return false;
    }

    static bool HandleLevel(ChatHandler* handler,
                            std::string const& target_name,
                            uint32 level)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p)
        {
            handler->PSendSysMessage("'%s' is not in-world. Run `.playerbot login %s` first.",
                                     target_name.c_str(), target_name.c_str());
            return false;
        }
        const Playerbot::BotId id = p->GetGUID().GetCounter();
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot â refusing to level a "
                                     "non-bot character via `.playerbot level`.", target_name.c_str());
            return false;
        }
        const uint32 maxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        if (level < 1 || level > maxLevel)
        {
            handler->PSendSysMessage("level must be 1..%u (configured MaxPlayerLevel).", maxLevel);
            return false;
        }
        const uint8 oldLevel = p->GetLevel();
        p->GiveLevel(uint8(level));
        p->InitTalentForLevel();           // refresh talent slots after the jump
        p->SetXP(0);                        // reset progress within the new level
        // A bot levelled by command never walks to a trainer: without this a Classic warrior levelled to 60 has no
        // Plate Mail skill (trainer-taught at 40) and the gear pass can't equip any of its chest/head/shoulder picks.
        const uint32 trainer_spells = Playerbot::API(p).learn_trainer_spells_free();

        // Auto-apply the spec's starter talent build so a freshly-leveled
        // bot has actual abilities, not blank talent slots. Skipped during
        // combat (apply_starter_talents returns Locked then) and below the
        // first talent unlock level (10).
        const char* talents_status = "skipped (combat or <lvl 10)";
        if (level >= 10 && !p->IsInCombat())
        {
            Playerbot::API api(p);
            const auto r = api.apply_starter_talents();
            talents_status = r == Playerbot::Result::Ok ? "applied" :
                             r == Playerbot::Result::Locked ? "locked (combat)" :
                             r == Playerbot::Result::NotKnown ? "no active trait config" :
                             "failed";
        }

        // Gear for the new level now rather than at the next 5-min backfill pass (a player's own alts keep their
        // gear; only an empty weapon slot is filled, see BotPopulationManager::BackfillOne).
        const char* gear_status = p->IsInCombat() ? "skipped (combat), next backfill pass" :
                                  Playerbot::Services::Population().RegearNow(p) ? "re-gear pass run" :
                                  "already level-appropriate";

        handler->PSendSysMessage("'%s' leveled %u â %u (max %u, %u trainer spells, talents: %s, gear: %s).",
                                 target_name.c_str(), oldLevel, level, maxLevel, trainer_spells, talents_status, gear_status);
        return true;
    }

    // .playerbot delete <name>
    //
    // Cleanup for test bots: refuses if the bot is in-world (use
    // `.playerbot logout` first), then unmarks, deletes the character row,
    // and removes the character cache entry. Hard-delete (deleteFinally
    // = true), so the row does not enter the soft-delete pool.
    static bool HandleDelete(ChatHandler* handler, std::string const& target_name)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            handler->PSendSysMessage("'%s' is not marked as a V2 bot â refusing to delete a "
                                     "non-bot character via `.playerbot delete`. Use core "
                                     "`.character delete` if you really mean it.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (ObjectAccessor::FindConnectedPlayer(g))
        {
            handler->PSendSysMessage("'%s' is currently in-world. Run `.playerbot logout %s` "
                                     "first, then re-run delete.", target_name.c_str(), target_name.c_str());
            return false;
        }

        // Unregister bot logic + unmark before deleting the character row so the
        // registry has no dangling reference if delete fails midway.
        if (Playerbot::Services::Registry().has(id))
        {
            Playerbot::Services::Scheduler().unregister_bot(id);
            Playerbot::Services::Snapshots().remove(id);
            Playerbot::Services::Registry().unregister_bot(id);
        }
        Playerbot::Services::Lifecycle().unmark_as_bot(id);

        // Resolve account id BEFORE deletion (cache lookup goes stale after).
        const uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(g);

        // Hard delete: deleteFinally=true forces CHAR_DELETE_REMOVE regardless
        // of CONFIG_CHARDELETE_METHOD, so test bots don't pollute the soft-
        // delete pool. updateRealmChars=true keeps realm character counts
        // accurate so .playerbot create's CHARACTERS_PER_ACCOUNT cap stays sane.
        Player::DeleteFromDB(g, accountId, /*updateRealmChars*/true, /*deleteFinally*/true);

        // Free a slot in the bot account pool so a future .playerbot create
        // can land here. No-op if accountId isn't a pool account (e.g.,
        // legacy bots created on the GM's account before the pool existed).
        Playerbot::Services::Accounts().note_character_removed(accountId);

        handler->PSendSysMessage("Deleted '%s' (guid %s) â unmarked + character row removed.",
                                 target_name.c_str(), g.ToString().c_str());
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return false;
        }
        // ConfigReader::reload re-reads playerbot.conf via sConfigMgr. HR-yes
        // keys take effect immediately on next read; HR-no keys are cached
        // by the subsystems that consumed them at init and won't update
        // until a worldserver restart. See playerbot.conf.dist for the per-
        // key HR tag.
        const bool ok = Playerbot::Services::Config().reload();
        handler->PSendSysMessage("playerbot.conf reload: %s", ok ? "ok" : "failed (see server log)");
        return true;
    }

    // .playerbot goto <name> <map> <x> <y> â set a manual cross-map travel
    // goal on a bot. Drives the SAME pipeline as the owner /goto whisper:
    // the builder synthesizes a relocation objective and the travel rules
    // route via walk/taxi/portal/ship/areatrigger â never a teleport.
    // Console-friendly so journeys are testable via SOAP.
    static bool HandleGoto(ChatHandler* handler, std::string const& target_name,
                           uint32 map_id, float x, float y)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.", target_name.c_str());
            return false;
        }
        Playerbot::BotAI* ai = Playerbot::Services::Registry().ai(id);
        if (!ai)
        {
            handler->PSendSysMessage("'%s' is not an active V2 bot session.", target_name.c_str());
            return false;
        }
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        ai->set_manual_travel(map_id, x, y, bot ? bot->GetPositionZ() : 0.f);
        handler->PSendSysMessage("'%s': travel goal set to map %u (%.0f, %.0f) â "
                                 "journey via flights/portals/ships.",
                                 target_name.c_str(), map_id, x, y);
        return true;
    }

    static bool HandleListRotations(ChatHandler* handler)
    {
        auto rotations = Playerbot::Combat::ListRotations();
        handler->PSendSysMessage("Registered rotations (%zu):", rotations.size());
        for (auto const& r : rotations)
            handler->PSendSysMessage("  cls=%u spec=%u rules=%zu", r.cls, r.spec, r.rules);
        return true;
    }

    static bool HandleResetStats(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 not initialized.");
            return true;
        }
        Playerbot::Services::Perf().reset();
        handler->SendSysMessage("PlayerbotV2 perf counters reset.");
        return true;
    }

    static bool HandlePopulation(ChatHandler* handler, Optional<std::string> sub)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        auto& pop = Playerbot::Services::Population();

        if (sub && (*sub == "force" || *sub == "rebalance"))
        {
            pop.ForceReconcile();
            handler->SendSysMessage("Population: forced reconciliation triggered.");
            return true;
        }

        auto snap = pop.Snapshot();
        handler->PSendSysMessage("Population: target=%u actual=%u", snap.total_target, snap.total_actual);
        handler->SendSysMessage("Bracket  | A_tgt A_cur | H_tgt H_cur | Delta");
        for (auto const& b : snap.buckets)
        {
            int32 a_delta = int32(b.alliance_target) - int32(b.alliance_actual);
            int32 h_delta = int32(b.horde_target) - int32(b.horde_actual);
            handler->PSendSysMessage("L%u-%u  |  %3u   %3u  |  %3u   %3u  | A%+d H%+d",
                uint32(b.level_lo), uint32(b.level_hi),
                b.alliance_target, b.alliance_actual,
                b.horde_target, b.horde_actual,
                a_delta, h_delta);
        }
        return true;
    }

    static bool HandlePipeline(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        { handler->SendSysMessage("Usage: .playerbot pipeline <character_name>"); return false; }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p) { handler->PSendSysMessage("'%s' not in-world.", target_name.c_str()); return false; }
        Playerbot::V2::Fleet::BotSetupPipeline pipe;
        pipe.Reset(p);
        handler->PSendSysMessage("Pipeline state for '%s' reset; will re-run on next population tick.",
                                  target_name.c_str());
        return true;
    }

    static bool HandleHealth(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::HealthReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleFleetHealth(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::FleetHealthOneScreen();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBgStats(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::BgOutcomeReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleBgInfo(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::BgInfoReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // #1B: `.playerbot wedges` â print the runtime wedge watchdog's current
    // active-wedge list grouped by root-cause category plus per-category
    // lifetime episode totals. Lets the operator see which bots are stuck
    // (and why) without tailing the log or noticing a frozen bot by eye.
    static bool HandleWedges(ChatHandler* handler)
    {
        const std::string report = Playerbot::Diagnostics::WedgesReport();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // `.playerbot bgjoin wsg|ab|av|<BattlemasterList id>`: queues the GM (and his group, through the same path as the battlemaster
    // window) for a battleground without a client packet. The 70205 client's Join Battle button sends an opcode the core could not
    // read, so this is the packet-independent way in; bots are filled by OnPlayerJoinedBgQueue exactly as for the window.
    static bool HandleBgJoin(ChatHandler* handler, std::string const& which)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("Run this in game: .playerbot bgjoin wsg|ab|av");
            return true;
        }
        uint32 listId = 0;
        if (which == "wsg") listId = 2;
        else if (which == "ab") listId = 3;
        else if (which == "av") listId = 1;
        else if (!which.empty() && which.find_first_not_of("0123456789") == std::string::npos) listId = uint32(std::stoul(which));
        if (!listId)
        {
            handler->SendSysMessage("Usage: .playerbot bgjoin wsg|ab|av|<BattlemasterList id>");
            return true;
        }
        handler->GetSession()->JoinBattlegroundQueue(BattlegroundMgr::BGQueueTypeId(uint16(listId), BattlegroundQueueIdType::Battleground, false, 0));
        return true;
    }

    // `.playerbot bgenter` / `.playerbot bgleave`: the Enter and Leave buttons of the battleground invite send nothing the core
    // receives on the 70205 client (trace 586f18cd), so these run the stock CMSG_BATTLEFIELD_PORT handler from a synthesized packet.
    // bgenter accepts the pending invite (the handler redirects the slot to the invited queue); bgleave leaves every queue.
    static bool SendBattlefieldPort(ChatHandler* handler, bool accept)
    {
        WorldSession* session = handler->GetSession();
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("Run this in game.");
            return true;
        }
        if (!player->InBattlegroundQueue())
        {
            handler->SendSysMessage("You are not in a battleground queue.");
            return true;
        }
        for (uint8 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
        {
            if (player->GetBattlegroundQueueTypeId(slot) == BATTLEGROUND_QUEUE_NONE)
                continue;
            WorldPackets::Battleground::BattlefieldPort port{ WorldPacket(CMSG_BATTLEFIELD_PORT) };
            port.Ticket.RequesterGuid = player->GetGUID();
            port.Ticket.Id = slot;
            port.Ticket.Type = WorldPackets::LFG::RideType::Battlegrounds;
            port.AcceptedInvite = accept;
            session->HandleBattleFieldPortOpcode(port);
            if (accept)
                break;  // the handler picks the invited queue; one accept is enough
        }
        return true;
    }
    static bool HandleBgEnter(ChatHandler* handler) { return SendBattlefieldPort(handler, true); }
    static bool HandleBgLeave(ChatHandler* handler) { return SendBattlefieldPort(handler, false); }

    // Control-zone diagnostic: `.playerbot bgzones <online-character>` lists
    // every CONTROL_ZONE gameobject on that character's map (EotS towers,
    // EPL-style proximity points) with its live capture-bar value,
    // controlling team, and a per-team census of players inside the capture
    // radius â applying the SAME eligibility tests the core ControlZone
    // heartbeat uses (alive + IsOutdoorPvPActive). Built 2026-06-11 to
    // root-cause "EotS towers never flip despite a team standing on them".
    static bool HandleBgZones(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        {
            handler->SendSysMessage("Usage: .playerbot bgzones <online_character>");
            return true;
        }
        Player* anchor = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!anchor)
        {
            handler->PSendSysMessage("Player '{}' not online.", target_name);
            return true;
        }
        Map* map = anchor->GetMap();

        ControlZoneCollector worker;
        TypeContainerVisitor<ControlZoneCollector, MapStoredObjectTypesContainer> visitor(worker);
        visitor.Visit(map->GetObjectsStore());

        // NOTE: this branch's ChatHandler::PSendSysMessage is printf-style;
        // build lines with fmt::format and send them whole (first version
        // used {} placeholders through PSendSysMessage and printed literal
        // braces with every argument dropped).
        handler->SendSysMessage(fmt::format("Control zones on map {} (instance {}): {}",
            map->GetId(), map->GetInstanceId(), worker.zones.size()).c_str());
        for (GameObject* go : worker.zones)
        {
            float const radius = float(go->GetGOInfo()->controlZone.radius);
            uint32 inRadius = 0, eligA = 0, eligH = 0, deadIn = 0, noPvp = 0, wrongPhase = 0;
            std::string phase_example;
            for (auto const& ref : map->GetPlayers())
            {
                Player* p = ref.GetSource();
                if (!p || !go->IsWithinDist(p, radius)) continue;
                ++inRadius;
                // Phase gate FIRST â the core's PlayerListSearcher filters by
                // the GO's PhaseShift before any eligibility test, so a
                // phase-mismatched player is INVISIBLE to the capture
                // heartbeat no matter how eligible. Live evidence 2026-06-11
                // (EotS inst 6): 18 horde inside BE Tower, bar frozen at
                // 50.0 â phase mismatch is the prime suspect (Chromie-time
                // phasing from the leveling fleet?).
                if (!p->InSamePhase(go))
                {
                    ++wrongPhase;
                    if (phase_example.empty())
                        phase_example = p->GetName();
                    continue;
                }
                if (!p->IsAlive())            { ++deadIn; continue; }
                if (!p->IsOutdoorPvPActive()) { ++noPvp;  continue; }
                if (p->GetTeamId() == TEAM_ALLIANCE) ++eligA; else ++eligH;
            }
            handler->SendSysMessage(fmt::format(
                "  go={} '{}' pos=({:.0f},{:.0f},{:.0f}) r={:.0f} value={:.1f} team={} | "
                "inside={} eligA={} eligH={} dead={} noPvp={} wrongPhase={}{} ticking={}",
                go->GetEntry(), go->GetGOInfo()->name,
                go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(),
                radius, go->GetControlZoneValue(),
                int32(go->GetControllingTeam()),
                inRadius, eligA, eligH, deadIn, noPvp, wrongPhase,
                phase_example.empty() ? std::string{}
                                      : fmt::format(" (e.g. {})", phase_example),
                go->HasFlag(GO_FLAG_NOT_SELECTABLE) ? 0 : 1).c_str());
        }
        return true;
    }

    // Team-coordinator plan dump (BG audit N60): one line per active
    // (BG instance, team) with the current order distribution.
    static bool HandleBgCoord(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 services not initialized.");
            return true;
        }
        const std::string report =
            Playerbot::Services::BgCoordinator().DebugDump();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // Form a party from named online players (bots or not):
    //   .playerbot group <leader> <member> [member...]
    // Testability surface for the dungeon/raid coordinator and group
    // behavior work â TC's own `.group join` can only ADD to an existing
    // group, so there was no console path to create one from scratch.
    // Mirrors LFGMgr's direct group creation (Group::Create + AddMember,
    // no invite round-trip â headless bot sessions can't click invites
    // from the console anyway).
    static bool HandleGroup(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->SendSysMessage(
                "Usage: .playerbot group <leader> <member> [member...]");
            return true;
        }
        std::vector<std::string> names;
        for (auto const& tok : Trinity::Tokenize(args, ' ', false))
            if (!tok.empty())
                names.emplace_back(tok);
        if (names.size() < 2)
        {
            handler->SendSysMessage(
                "Usage: .playerbot group <leader> <member> [member...]");
            return true;
        }
        // Pure-bot groups never block formation: the BotGroupBuilder /
        // social-invite bot logic constantly re-groups idle bots, so any test
        // group has to be able to YANK its members out of those. Groups
        // containing a human are left untouched (skip with a message).
        auto detach_if_bot_group = [&](Player* p) -> bool
        {
            Group* g = p->GetGroup();
            if (!g) return true;
            for (auto const& slot : g->GetMemberSlots())
                if (!Playerbot::Services::Lifecycle().is_bot(
                        slot.guid.GetCounter()))
                    return false;   // human group â hands off
            g->RemoveMember(p->GetGUID());
            return p->GetGroup() == nullptr;
        };
        Player* leader = ObjectAccessor::FindPlayerByName(names[0]);
        if (!leader)
        {
            handler->PSendSysMessage("Leader '%s' is not online.",
                                     names[0].c_str());
            return true;
        }
        Group* grp = leader->GetGroup();
        if (grp && !detach_if_bot_group(leader))
        {
            handler->PSendSysMessage(
                "Leader '%s' is in a group with humans â not touching it.",
                names[0].c_str());
            return true;
        }
        grp = leader->GetGroup();
        if (!grp)
        {
            grp = new Group();
            if (!grp->Create(leader))
            {
                delete grp;
                handler->PSendSysMessage("Group creation failed for '%s'.",
                                         names[0].c_str());
                return true;
            }
            sGroupMgr->AddGroup(grp);
        }
        uint32 added = 0;
        for (size_t i = 1; i < names.size(); ++i)
        {
            Player* m = ObjectAccessor::FindPlayerByName(names[i]);
            if (!m)
            { handler->PSendSysMessage("'%s' is not online â skipped.", names[i].c_str()); continue; }
            if (m->GetGroup() == grp)
            { ++added; continue; }
            if (m->GetGroup() && !detach_if_bot_group(m))
            { handler->PSendSysMessage("'%s' is grouped with humans â skipped.", names[i].c_str()); continue; }
            if (grp->IsFull() && !grp->isRaidGroup())
                grp->ConvertToRaid();
            if (grp->AddMember(m))
                ++added;
            else
                handler->PSendSysMessage("AddMember failed for '%s'.", names[i].c_str());
        }
        grp->BroadcastGroupUpdate();
        handler->PSendSysMessage("Group: leader %s + %u member(s).",
                                 leader->GetName().c_str(), added);
        return true;
    }

    // .playerbot dungeontest <lfg_dungeon_id> [level]
    //
    // Stands up (or refreshes) a FIXED pure-bot dungeon squad â 1 tank
    // (Prot Warrior), 1 healer (Holy Priest), 3 DPS (Frost Mage / Sin
    // Rogue / BM Hunter), all Alliance Humans â then LFG-queues all five
    // for the named dungeon so the dungeon-finder forms the group, ports
    // them in, and the bots auto-run it (is_lfg auto-activation). This is
    // the headless equivalent of a player queuing a 5-stack, and the only
    // way to verify dungeon play without a human to whisper /lfg.
    //
    // Idempotent + self-converging: members that aren't in-world yet are
    // created/logged-in this call (report says "preparing"); re-run once
    // they're online to spec/gear/level + queue. The target level is
    // clamped into the dungeon's ContentTuning bracket so the LFG level
    // gate (LFGMgr::GetLockedDungeons) can't reject the queue.
    // .playerbot dungeonreset â force the dungeontest squad out of any
    // instance back to the Alliance staging area and clear their LFG state,
    // so the next `dungeontest` re-queues a fresh run. The run-protection
    // guard in dungeontest (correctly) refuses to disturb a live run; this is
    // the explicit "tear the current run down" escape hatch for the harness â
    // e.g. after a server restart reloaded the squad mid-run (their saved
    // position is inside the instance, but last_lfg_dungeon_id resets on
    // relogin so they can no longer auto-run), or simply to retry from
    // scratch. This is a deliberate test reset, NOT a stuck-bot rescue.
    // Dump every registered DungeonScript as a machine-readable line for the
    // offline route generator (tools/gen_dungeon_routes.py). One line per script:
    //   DDUMP|<map_id>|<lfg_dungeonId>|<name>|<boss1,boss2,...>
    // map_id + bosses come from the script (get_advice with an empty snapshot â
    // the same safe static-extraction the dungeon smoketest uses); lfg_dungeonId
    // is the first LFGDungeons.db2 row on that map (used to look up the LFG
    // entrance in wc_world.lfg_dungeon_template). 0 => no LFG entry (generator
    // falls back / skips). Read-only.
    static bool HandleDungeonDump(ChatHandler* handler)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        auto& mgr = Services::Dungeons();
        uint32 count = 0;
        mgr.for_each_script([&](DungeonScript const& script)
        {
            BotSnapshot empty_snap{};
            BotSnapshotView empty_view(empty_snap);
            DungeonAdvice a = script.get_advice(empty_view);

            uint32 lfg_id = 0;
            for (LFGDungeonsEntry const* d : sLFGDungeonsStore)
                if (d && uint32(d->MapID) == script.map_id())
                { lfg_id = d->ID; break; }

            std::string bosses;
            for (uint32 b : a.bosses)
            {
                if (!bosses.empty()) bosses += ",";
                bosses += std::to_string(b);
            }
            handler->PSendSysMessage("DDUMP|%u|%u|%s|%s",
                script.map_id(), lfg_id, script.name(), bosses.c_str());
            ++count;
        });
        handler->PSendSysMessage("DDUMP_END|%u scripts", count);
        return true;
    }

    // .playerbot reloadroutes â hot-reload ALL bot nav data from the shared DB
    // without a server restart, SOAP-callable so the world editor (and test
    // harnesses) can activate committed route/road changes instantly:
    //  * dungeon route waypoints ({SharedDb}.playerbot_dungeon_routes) â
    //    DungeonScriptMgr::LoadGeneratedRoutes builds a fresh table and
    //    atomically swaps it under the routes mutex (bot-worker readers keep a
    //    coherent snapshot); the next GetAdvice tick serves the new routes.
    //  * handcrafted roads ({SharedDb}.handcrafted_road) â storage reload bumps
    //    the generation counter and the road GRAPH cache is invalidated, so the
    //    next ComputeRoute rebuilds from the new segments. NOTE: navmesh AREA
    //    tagging (NAV_AREA_ROAD slope-cost) on already-loaded maps still needs
    //    `.reload handcrafted_road apply <mapId>` â tag flips are destructive
    //    and per-map, so they stay an explicit step.
    // Runs on the world thread (command context) â same thread as the startup
    // loads, so the synchronous DB queries are safe here.
    static bool HandleReloadRoutes(ChatHandler* handler)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        size_t const wps   = Services::Dungeons().LoadGeneratedRoutes();
        size_t const links = Services::Dungeons().LoadNavLinks();

        HandcraftedRoadStorage::LoadFromDB();
        TerrainMgrDetail::ClearAppliedHandcraftedRoads();
        sHandcraftedRoadGraph->InvalidateAll();

        handler->PSendSysMessage(
            "reloadroutes: %u dungeon route waypoint(s) + %u traversal link(s) reloaded "
            "(live next tick); %u road segment(s) across %u map(s) reloaded (graph cache "
            "invalidated). Navmesh road-area tags on loaded maps need "
            "`.reload handcrafted_road apply <mapId>`.",
            uint32(wps), uint32(links), uint32(HandcraftedRoadStorage::SegmentCount()),
            uint32(HandcraftedRoadStorage::MapCount()));
        return true;
    }

    // .playerbot killentry <botname> <entry> â TEST utility: the named bot
    // kills the nearest LIVE creature of <entry> within 300y (encounter credit
    // goes to the bot, so dm_credit fires exactly as in a real kill). Exists so
    // boss-progression states can be advanced without clearing every earlier
    // encounter â e.g. validating the Lord Serpentis traversal link without
    // first killing four bosses. GM-gated.
    static bool HandleKillEntry(ChatHandler* handler, std::string const& botname,
                                uint32 entry)
    {
        Player* p = ObjectAccessor::FindConnectedPlayerByName(botname);
        if (!p)
        { handler->PSendSysMessage("killentry: bot '%s' not online.", botname.c_str()); return false; }
        Creature* found = nullptr;
        {
            std::list<Creature*> list;
            Trinity::AllCreaturesOfEntryInRange chk(p, entry, 300.0f);
            Trinity::CreatureListSearcher<Trinity::AllCreaturesOfEntryInRange> searcher(p, list, chk);
            Cell::VisitAllObjects(p, searcher, 300.0f);
            float best = 1e18f;
            for (Creature* c : list)
                if (c && c->IsAlive())
                {
                    const float d = p->GetExactDistSq(c);
                    if (d < best) { best = d; found = c; }
                }
        }
        if (!found)
        { handler->PSendSysMessage("killentry: no live creature %u within 300y of '%s'.", entry, botname.c_str()); return false; }
        Unit::Kill(p, found);
        handler->PSendSysMessage("killentry: '%s' killed %s (entry %u) at (%.1f, %.1f, %.1f).",
            botname.c_str(), found->GetName().c_str(), entry,
            found->GetPositionX(), found->GetPositionY(), found->GetPositionZ());
        return true;
    }

    static bool HandleDungeonReset(ChatHandler* handler)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        static char const* const kNames[5] =
            { "Dungtank", "Dunghealer", "Dungmage", "Dungrogue", "Dunghunter" };
        static constexpr float kSX = -8985.0f, kSY = 511.0f, kSZ = 79.8f;
        uint32 moved = 0, cleared = 0;
        std::string status;
        for (char const* nm : kNames)
        {
            BotId id = ResolveCharacterId(nm);
            if (!id) { status += fmt::format("{}=absent ", nm); continue; }
            ObjectGuid og = ObjectGuid::Create<HighGuid::Player>(id);
            // Clear LFG state regardless of online status so the next
            // solo-queue starts from a clean slate (mirrors the LeaveLfg in
            // the dungeontest queue loop).
            sLFGMgr->LeaveLfg(og);
            ++cleared;
            Player* p = ObjectAccessor::FindConnectedPlayer(og);
            if (!p) { status += fmt::format("{}=offline ", nm); continue; }
            // Drop any (LFG or pure-bot) group so the re-queue is solo + clean.
            if (Group* grp = p->GetGroup())
                grp->RemoveMember(og);
            p->CombatStop(true);
            p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
            ++moved;
            status += fmt::format("{}=staged ", nm);
        }
        handler->PSendSysMessage(
            "dungeonreset: %u LFG-cleared, %u staged to (%.0f,%.0f). [%s]",
            cleared, moved, kSX, kSY, status.c_str());
        return true;
    }

    static bool HandleDungeonTest(ChatHandler* handler, uint32 dungeonId,
                                  Optional<uint32> levelArg)
    {
        using namespace Playerbot;
        if (!Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        LFGDungeonsEntry const* dun = sLFGDungeonsStore.LookupEntry(dungeonId);
        if (!dun)
        { handler->PSendSysMessage("dungeontest: no LFGDungeons row %u.", dungeonId); return false; }

        // Resolve the dungeon's level bracket from ContentTuning (12.0 has
        // no MinLevel/MaxLevel on LFGDungeons â it scales via ContentTuning,
        // the exact source LFGMgr's level gate reads). Pass an empty redirect
        // mask (Chromie CtrOptions is a stub on this server anyway).
        int16 minL = 1, maxL = uint8(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        if (Optional<ContentTuningLevels> lv =
                sDB2Manager.GetContentTuningData(dun->ContentTuningID, {}))
        { if (lv->MaxLevel > 0) { minL = lv->MinLevel; maxL = lv->MaxLevel; } }
        uint32 level = levelArg ? *levelArg : uint32(maxL);
        if (level < uint32(minL)) level = uint32(minL);
        if (level > uint32(maxL)) level = uint32(maxL);

        struct Spec { char const* name; uint8 cls; uint32 spec; Role role; };
        // Race 1 = Human (Alliance); all five classes are legal for Human in
        // 12.0. Spec ids are ChrSpecialization.db2 (Prot 73 / Holy 257 /
        // Frost 64 / Assassination 259 / Beast Mastery 253).
        static const Spec kSquad[5] = {
            { "Dungtank",   CLASS_WARRIOR, 73u,  Role::Tank   },
            { "Dunghealer", CLASS_PRIEST,  257u, Role::Healer },
            { "Dungmage",   CLASS_MAGE,    64u,  Role::Dps    },
            { "Dungrogue",  CLASS_ROGUE,   259u, Role::Dps    },
            { "Dunghunter", CLASS_HUNTER,  253u, Role::Dps    },
        };

        // --- Run protection (idempotency) ---------------------------------
        // If the squad is already inside an active LFG run for THIS dungeon,
        // re-invoking the command (the harness re-runs it to monitor progress)
        // must NOT disturb it. The per-member position-reset below ejects any
        // bot found on a dungeon map to the staging area â which, on a squad
        // that had JUST been ported in, tore the live run apart and desynced
        // LFG (the group state stayed "In dungeon" while the bodies were yanked
        // back to map 0, so the run never advanced). Detect the live run by LFG
        // state (LFG_STATE_DUNGEON) on the dungeon's own map and short-circuit:
        // report and leave the run alone. Only when the run has genuinely
        // collapsed (the instance is empty of the squad) do we fall through to
        // re-stage + re-queue â and LeaveLfg() below clears any stale state.
        {
            uint32 inRun = 0;
            std::string rs;
            for (Spec const& m : kSquad)
            {
                BotId rid = ResolveCharacterId(m.name);
                if (!rid) continue;
                ObjectGuid rog = ObjectGuid::Create<HighGuid::Player>(rid);
                Player* rp = ObjectAccessor::FindConnectedPlayer(rog);
                if (!rp) continue;
                if (sLFGMgr->GetState(rog) == lfg::LFG_STATE_DUNGEON &&
                    rp->GetMap() && rp->GetMap()->IsDungeon() &&
                    int16(rp->GetMapId()) == dun->MapID)
                {
                    ++inRun;
                    rs += fmt::format("{}=running ", m.name);
                }
            }
            // tank + healer + at least one dps still inside â treat as a live
            // run and refuse to touch it (re-run is a no-op while running).
            if (inRun >= 3)
            {
                handler->PSendSysMessage(
                    "dungeontest '%s' (id %u): squad already in an ACTIVE run â "
                    "%u/5 in the dungeon. Not disturbing. [%s]",
                    dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId,
                    inRun, rs.c_str());
                return true;
            }
        }

        // Detach a bot from any pure-bot group so it can solo-queue LFG (the
        // social-invite bot logic constantly re-groups idle bots). Humans untouched.
        auto detach_if_bot_group = [&](Player* p)
        {
            Group* g = p->GetGroup();
            if (!g) return;
            for (auto const& slot : g->GetMemberSlots())
                if (!Services::Lifecycle().is_bot(slot.guid.GetCounter()))
                    return;
            g->RemoveMember(p->GetGUID());
        };

        std::vector<Player*> ready;
        uint32 preparing = 0, created = 0;
        std::string status;
        for (Spec const& m : kSquad)
        {
            BotId id = ResolveCharacterId(m.name);
            bool justCreated = false;
            if (!id)
            {
                auto r = V2::BotCharacterFactory::Create(
                    handler->GetSession(), m.name, /*race Human*/ 1, m.cls, /*gender*/ 0);
                if (!r.ok)
                { status += fmt::format("{}=createfail({}) ", m.name, r.reason); continue; }
                id = r.guid.GetCounter();
                ++created;
                justCreated = true;
            }
            Services::Lifecycle().mark_as_bot(id);
            // Shield the squad from the population overflow-kick: at a low
            // TotalTarget these dedicated test bots are pool inventory and get
            // logged out the instant online > target â which made them flicker
            // offline mid-setup and never reach the LFG queue. A rolling 10-min
            // kick-protection lease (refreshed every command call) takes them
            // off the shaper's books until they're LFG-queued (the queue state
            // is itself a kick exemption). Same lease BotQueueFiller gives JIT
            // BG/LFG match-fill spawns.
            Services::Population().ProtectFromKick(uint64(id), 10u * 60u * 1000u);

            // Do NOT log a just-created char in on the same call: the character
            // row isn't DB-committed yet, so BeginLogin loads nothing and the
            // BotSession sits in sessions_ forever as an "in flight" zombie that
            // never enters world. Defer login to the next call (row committed by
            // then). For an existing-but-offline member: if a (possibly zombie)
            // session is already in flight, clear it first â a fresh login on a
            // committed row reliably lands the bot in-world; otherwise submit one.
            ObjectGuid og = ObjectGuid::Create<HighGuid::Player>(id);
            Player* p = ObjectAccessor::FindConnectedPlayer(og);
            if (!p)
            {
                if (justCreated)
                { ++preparing; status += fmt::format("{}=created ", m.name); continue; }
                if (Services::SessionMgr().IsHeadless(og))
                {
                    Services::SessionMgr().LogoutBot(og);   // drop stale in-flight; relogin next call
                    status += fmt::format("{}=reset ", m.name);
                }
                else
                {
                    Services::SessionMgr().LoginBot(og);    // fresh submit on a committed row
                    status += fmt::format("{}=login ", m.name);
                }
                ++preparing;
                continue;
            }
            // Position reset: runs before the alive/combat gates so that bots
            // stranded inside a dungeon or BG from a prior run are staged out
            // first (mobs would re-aggro instantly and keep them in incombat
            // indefinitely if we tried to queue them from inside the instance).
            static constexpr float kSX = -8985.0f, kSY = 511.0f, kSZ = 79.8f;
            if (p->GetMap()->IsDungeon() || p->GetMap()->IsBattleground())
            {
                // Stranded in old instance â eject to the Alliance staging area.
                p->CombatStop(true);
                p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
                ++preparing;
                status += fmt::format("{}=staged ", m.name);
                continue;
            }
            // Overworld bad state: in water, in combat, or underground/at-sea-level.
            // Teleport is skipped when already near staging to avoid loop, but
            // CombatStop is ALWAYS called when in combat â stale refs from a prior
            // dungeon run can persist after the staging teleport and must be cleared.
            {
                float px, py, pz;
                p->GetPosition(px, py, pz);
                const float sdx = px - kSX, sdy = py - kSY;
                const bool nearStaging = sdx * sdx + sdy * sdy < 500.0f * 500.0f;
                if (p->IsInWater() || p->IsInCombat() || pz < 5.0f)
                {
                    p->CombatStop(true);
                    if (!nearStaging)
                    {
                        p->TeleportTo(0, kSX, kSY, kSZ, 0.0f);
                        ++preparing;
                        status += fmt::format("{}=staged ", m.name);
                        continue;
                    }
                    // Near staging but still in combat (stale refs) â CombatStop
                    // was called above; re-check after a tick via another invocation.
                    if (p->IsInCombat())
                    { ++preparing; status += fmt::format("{}=incombat ", m.name); continue; }
                }
            }
            // Alive-gate: a dead/ghost member can't LFG-queue (the API rejects
            // it), so the finder forms an incomplete group and the proposal
            // lapses â the squad reverts to open-world questing and the run
            // never starts. Revive any downed member in place (these are
            // dedicated test bots; a real player would corpse-run, but for the
            // harness an instant rez is correct) and don't count it ready until
            // it is actually alive again.
            if (!p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                p->SpawnCorpseBones();
                ++preparing;
                status += fmt::format("{}=revive ", m.name);
                continue;
            }

            // Setup: level + gear via the standard pipeline, then force the
            // role spec LAST so the bot actually tanks/heals (role inference
            // reads GetPrimarySpecialization). RunFor is incremental and
            // persists its own per-bot state â the pop manager calls it ONCE
            // per tick. We mirror that: exactly one step per command invocation
            // (this command is idempotent/re-run, so setup converges across
            // calls). A tight RunFor loop here ran the full gear+travel pipeline
            // for 5 bots synchronously on the world thread in a single tick and
            // contributed to a world-thread-hang crash â never loop it. No
            // Reset(): that would wipe progress every call and never finish.
            // Force exact level match before running the setup pipeline: if the
            // bot leveled past the target (e.g. gained XP fighting mobs while
            // ejected in the overworld), GiveLevel it back down. The LFG
            // eligibility gate rejects anyone above the dungeon bracket max.
            if (uint32(p->GetLevel()) != level)
                p->GiveLevel(uint8(level));
            const bool setupDone = V2::Fleet::BotSetupPipeline{}.RunFor(p, uint8(level));
            if (!setupDone || uint32(p->GetLevel()) != level)
            { ++preparing; status += fmt::format("{}=setup ", m.name); continue; }
            if (ChrSpecializationEntry const* se = sChrSpecializationStore.LookupEntry(m.spec))
                if (se->ClassID == p->GetClass() &&
                    uint32(p->GetPrimarySpecialization()) != m.spec)
                {
                    p->ActivateTalentGroup(se);
                    API(p).apply_starter_talents();
                }
            ready.push_back(p);
        }

        if (ready.size() < 5)
        {
            handler->PSendSysMessage(
                "dungeontest '%s' (id %u, lvl %u, bracket %d-%d): %u/5 ready, "
                "%u created, %u preparing. Re-run in a few seconds to queue. [%s]",
                dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId, level,
                int(minL), int(maxL), uint32(ready.size()), created, preparing,
                status.c_str());
            return true;
        }

        // All five geared/specced + in-world â solo-queue each for the
        // dungeon with its role. The finder matches the 5, sends a proposal
        // (bots auto-accept, idle:lfg_proposal_accept), ports them in, and
        // dungeon-run auto-activates for the whole is_lfg group.
        uint32 queued = 0;
        for (size_t i = 0; i < 5; ++i)
        {
            Player* p = ready[i];
            BotId id = p->GetGUID().GetCounter();
            detach_if_bot_group(p);
            // Clear any stale LFG state from a prior run (e.g. PROPOSAL or
            // DUNGEON that persisted across a server restart). Without this,
            // lfg_queue() silently rejects the bot if it's already in a
            // terminal-but-stale LFG state.
            sLFGMgr->LeaveLfg(p->GetGUID());
            IntentQueue* q = Services::Registry().intents(id);
            IntentId* next = Services::Registry().next_intent_id(id);
            if (!q || !next) continue;
            BotIntentEmitter emit(q, id, /*source*/ 0, next);
            if (emit.lfg_queue(dungeonId, kSquad[i].role))
            {
                if (BotAI* ai = Services::Registry().ai(id))
                    ai->set_last_lfg_dungeon_id(dungeonId);
                ++queued;
            }
        }
        handler->PSendSysMessage(
            "dungeontest '%s' (id %u): squad ready at lvl %u â LFG-queued %u/5 "
            "(tank+healer+3dps). Finder will form the group + port them in.",
            dun->Name.Str[sWorld->GetDefaultDbcLocale()], dungeonId, level, queued);
        return true;
    }

    // PvE group-coordinator plan dump: one line per coordinated
    // dungeon/raid group with the current duty distribution.
    static bool HandlePveCoord(ChatHandler* handler)
    {
        if (!Playerbot::Services::Initialized())
        {
            handler->SendSysMessage("PlayerbotV2 services not initialized.");
            return true;
        }
        const std::string report =
            Playerbot::Services::PveCoordinator().DebugDump();
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    // ----- Road-aware pathfinding telemetry --------------------------------
    //
    // dtQueryFilterTC maintains atomic counters incremented per
    // CalculatePath via dtQueryFilterTC::TallyPath (called from
    // PathGenerator::CalculatePath after the path stabilises).
    //
    // Modes:
    //   .playerbot roadstats              â global summary
    //   .playerbot roadstats <mapId>      â single-map breakdown
    //   .playerbot roadstats list         â table of every map with tallied paths
    //
    // Reports: paths run, paths with â¥1 road poly (% of total),
    // road poly fraction, paths in instance, paths with road bonus
    // disabled, paths with slope penalty > 10%.

    static std::string FormatRoadStatsLine(char const* label,
                                           dtQueryFilterTC::RoadStats const& st)
    {
        return Trinity::StringFormat(
            "{} paths_run={} with_road={} ({:.1f}%) disabled={} ({:.1f}%) "
            "in_instance={} slope_penalty={} ({:.1f}%) "
            "road_polys={}/{} ({:.2f}%)",
            label,
            st.pathsRun,
            st.pathsWithRoadPoly,
            st.pathsRun ? 100.0 * st.pathsWithRoadPoly / st.pathsRun : 0.0,
            st.pathsRoadBonusDisabled,
            st.pathsRun ? 100.0 * st.pathsRoadBonusDisabled / st.pathsRun : 0.0,
            st.pathsInInstance,
            st.pathsWithSlopePenalty,
            st.pathsRun ? 100.0 * st.pathsWithSlopePenalty / st.pathsRun : 0.0,
            st.roadPolysVisited, st.totalPolysVisited,
            st.totalPolysVisited ? 100.0 * st.roadPolysVisited / st.totalPolysVisited : 0.0);
    }

    static bool HandleRoadStats(ChatHandler* handler, Optional<std::string> argOpt)
    {
        std::string arg = argOpt ? *argOpt : std::string{};
        // trim
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t'))
            arg.erase(arg.begin());
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t'))
            arg.pop_back();

        if (arg == "list")
        {
            auto maps = dtQueryFilterTC::ListMapsWithStats();
            if (maps.empty())
            {
                handler->SendSysMessage("Roadstats list: no paths tallied yet.");
                return true;
            }
            std::vector<std::pair<uint32, dtQueryFilterTC::RoadStats>> entries;
            entries.reserve(maps.size());
            for (uint32 m : maps)
                entries.emplace_back(m, dtQueryFilterTC::SampleStatsForMap(m));
            std::sort(entries.begin(), entries.end(),
                [](auto const& a, auto const& b) { return a.second.pathsRun > b.second.pathsRun; });

            handler->PSendSysMessage("Roadstats list: %zu maps with tallied paths",
                                     entries.size());
            for (auto const& [m, st] : entries)
            {
                std::string label = Trinity::StringFormat("  map={}:", m);
                handler->SendSysMessage(FormatRoadStatsLine(label.c_str(), st));
            }
            return true;
        }

        if (!arg.empty())
        {
            uint32 mapId = 0;
            try { mapId = static_cast<uint32>(std::stoul(arg)); }
            catch (...) { mapId = 0; }
            if (mapId == 0)
            {
                handler->PSendSysMessage(
                    "Roadstats: unknown arg '%s'. Use no arg (global), <mapId>, or 'list'.",
                    arg.c_str());
                return false;
            }
            dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStatsForMap(mapId);
            if (st.pathsRun == 0)
            {
                handler->PSendSysMessage("Roadstats map=%u: no paths tallied for this map.", mapId);
                return true;
            }
            std::string label = Trinity::StringFormat("Roadstats map={}:", mapId);
            handler->SendSysMessage(FormatRoadStatsLine(label.c_str(), st));
            return true;
        }

        dtQueryFilterTC::RoadStats st = dtQueryFilterTC::SampleStats();
        handler->SendSysMessage(FormatRoadStatsLine("Roadstats:", st));
        return true;
    }

    static bool HandleRoadReset(ChatHandler* handler)
    {
        dtQueryFilterTC::ResetStats();
        handler->SendSysMessage("Roadstats: counters reset.");
        return true;
    }

    // A/B compare: run the same pathfind twice â once with road bonus,
    // once without â and report the difference in length + road poly
    // count. Lets the operator confirm road bias is producing visibly
    // different routes for a specific destination.
    //
    // Usage:
    //   .playerbot pathcompare <x> <y> <z>
    //   .playerbot pathcompare preset <name>
    //   .playerbot pathcompare preset       (lists available presets)
    static bool HandlePathCompare(ChatHandler* handler, Optional<std::string> argOpt)
    {
        Player* p = handler->GetPlayer();
        if (!p)
        {
            handler->SendSysMessage("Pathcompare: must be run in-world (needs a source unit).");
            return false;
        }

        struct Preset {
            char const* name;
            uint32 mapId;
            float x, y, z;
            char const* note;
        };
        constexpr Preset presets[] = {
            {"stormwind-goldshire",   0, -9460.0f,    55.0f,  56.0f,
                "Eastern Kingdoms south road from Stormwind gate to Goldshire"},
            {"goldshire-westfall",    0, -10650.0f,  1052.0f,  37.0f,
                "Sentinel Hill â long cobblestone road south"},
            {"stormwind-darkshire",   0, -10590.0f,  -1136.0f, 48.0f,
                "Duskwood Darkshire â winding forest road"},
            {"ironforge-kharanos",    0, -5500.0f,    -710.0f, 350.0f,
                "Dun Morogh road from Ironforge to Kharanos"},
            {"orgrimmar-razorhill",   1, 326.0f,    -4690.0f,  10.0f,
                "Durotar south road from Orgrimmar gate to Razor Hill"},
            {"orgrimmar-crossroads",  1, -440.0f,   -2570.0f,  96.0f,
                "Barrens north road from Orgrimmar to The Crossroads"},
            {"thunderbluff-bloodhoof",1, -2360.0f,  -370.0f,   -10.0f,
                "Mulgore south road from Thunder Bluff to Bloodhoof Village"},
            {"darnassus-rutth",       1, 9947.0f,    2570.0f, 1316.0f,
                "Teldrassil south road from Darnassus to Rut'theran"},
        };

        std::string arg = argOpt ? *argOpt : std::string{};
        while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t'))
            arg.erase(arg.begin());
        while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t'))
            arg.pop_back();

        float x = 0.0f, y = 0.0f, z = 0.0f;

        if (arg.rfind("preset", 0) == 0)
        {
            std::string presetName;
            auto sp = arg.find(' ');
            if (sp != std::string::npos)
                presetName = arg.substr(sp + 1);

            if (presetName.empty())
            {
                handler->SendSysMessage("Pathcompare presets:");
                for (auto const& pr : presets)
                    handler->PSendSysMessage("  %s (map %u) â %s",
                                             pr.name, pr.mapId, pr.note);
                return true;
            }

            Preset const* chosen = nullptr;
            for (auto const& pr : presets)
                if (presetName == pr.name) { chosen = &pr; break; }
            if (!chosen)
            {
                handler->PSendSysMessage(
                    "Pathcompare: unknown preset '%s'. Use .playerbot pathcompare preset (no name) to list.",
                    presetName.c_str());
                return false;
            }
            if (p->GetMapId() != chosen->mapId)
                handler->PSendSysMessage(
                    "Pathcompare: warning â you are on map %u, preset is for map %u. Pathfind may fail.",
                    p->GetMapId(), chosen->mapId);
            x = chosen->x; y = chosen->y; z = chosen->z;
        }
        else if (std::sscanf(arg.c_str(), "%f %f %f", &x, &y, &z) != 3)
        {
            handler->SendSysMessage(
                "Pathcompare: usage: <x> <y> <z> | preset <name> | preset (list)");
            return false;
        }

        auto runOne = [&](bool disableBonus) -> std::tuple<bool, float, uint32> {
            PathGenerator pg(p);
            pg.SetDisableRoadBonus(disableBonus);
            bool ok = pg.CalculatePath(x, y, z, false);
            float len = pg.GetPathLength();
            uint32 nPoints = static_cast<uint32>(pg.GetPath().size());
            return { ok, len, nPoints };
        };

        dtQueryFilterTC::RoadStats stBefore = dtQueryFilterTC::SampleStats();
        auto [okWith, lenWith, ptsWith] = runOne(false);
        dtQueryFilterTC::RoadStats stMid = dtQueryFilterTC::SampleStats();
        auto [okWithout, lenWithout, ptsWithout] = runOne(true);
        dtQueryFilterTC::RoadStats stAfter = dtQueryFilterTC::SampleStats();

        const uint64 roadWith    = stMid.roadPolysVisited    - stBefore.roadPolysVisited;
        const uint64 roadWithout = stAfter.roadPolysVisited  - stMid.roadPolysVisited;
        const uint64 totalWith   = stMid.totalPolysVisited   - stBefore.totalPolysVisited;
        const uint64 totalWithout= stAfter.totalPolysVisited - stMid.totalPolysVisited;

        handler->PSendSysMessage("Pathcompare to (%.1f, %.1f, %.1f):", x, y, z);
        handler->PSendSysMessage(
            "  with road:    ok=%d len=%.1fy points=%u road_polys=%llu/%llu",
            okWith ? 1 : 0, lenWith, ptsWith,
            static_cast<unsigned long long>(roadWith),
            static_cast<unsigned long long>(totalWith));
        handler->PSendSysMessage(
            "  without road: ok=%d len=%.1fy points=%u road_polys=%llu/%llu",
            okWithout ? 1 : 0, lenWithout, ptsWithout,
            static_cast<unsigned long long>(roadWithout),
            static_cast<unsigned long long>(totalWithout));

        const float lenDelta = lenWith - lenWithout;
        const int64 roadDelta = static_cast<int64>(roadWith) - static_cast<int64>(roadWithout);
        char const* verdict;
        if (roadDelta > 0 && std::abs(lenDelta) < 5.0f)
            verdict = "ROAD BIAS ACTIVE â same length, road-only detour";
        else if (roadDelta > 0)
            verdict = "ROAD BIAS ACTIVE â picked road route";
        else if (roadDelta == 0 && std::abs(lenDelta) < 0.5f)
            verdict = "no road influence on this route";
        else
            verdict = "OFF-ROAD PREFERRED â slope or no roads nearby";
        handler->PSendSysMessage("  delta_length=%+.1fy delta_road_polys=%+lld  -> %s",
                                 lenDelta, static_cast<long long>(roadDelta), verdict);
        return true;
    }

    static bool HandleDiag(ChatHandler* handler, std::string const& target_name)
    {
        if (target_name.empty())
        { handler->SendSysMessage("Usage: .playerbot diag <character_name>"); return false; }
        Player* p = ObjectAccessor::FindConnectedPlayerByName(target_name);
        if (!p) { handler->PSendSysMessage("'%s' not in-world.", target_name.c_str()); return false; }
        const std::string report =
            Playerbot::Diagnostics::DiagBot(p->GetGUID().GetCounter());
        for (auto const& line : Trinity::Tokenize(report, '\n', true))
            handler->SendSysMessage(std::string{line});
        return true;
    }

    static bool HandleReseed(ChatHandler* handler, Optional<std::string> confirm_arg)
    {
        // Destructive operation â require explicit "confirm" arg to run, so a
        // typo of `.playerbot reseed` doesn't wipe the entire distribution
        // metadata. Resets distribution_level + setup_pipeline_state for ALL
        // V2 characters; the next population tick re-shapes the fleet from
        // scratch and re-runs the per-bot setup pipeline.
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        if (!confirm_arg || *confirm_arg != "confirm")
        {
            handler->SendSysMessage(
                "Reseed: clears distribution_level + setup_pipeline_state on every V2 bot, "
                "then triggers a full re-shape. Run `.playerbot reseed confirm` to proceed.");
            return true;
        }

        CharacterDatabase.PExecute(
            "UPDATE playerbot_v2_character SET distribution_level=0, setup_pipeline_state=0");
        Playerbot::Services::Population().ForceReconcile();
        handler->SendSysMessage("Reseed: wiped distribution + pipeline state. Force-reconciling now.");
        return true;
    }

    // .playerbot wipefleet confirm
    //
    // Hard-deletes EVERY V2 bot character: full DeleteFromDB on each
    // character_guid, drops the playerbot_v2_character rows, frees pool
    // account slots. The next population tick sees an empty fleet and
    // respawns from scratch with the current pipeline + composition + gear
    // generator + talent seed - so existing bots that pre-date today's
    // fixes (Norithyn-style equip-swap broken, conjure-loop, etc.) get
    // replaced with fresh bots that reflect the current system state.
    //
    // Online bots are kicked via SessionMgr::LogoutBot first; their actual
    // DB-side deletion happens on the SAME call (DeleteFromDB tolerates a
    // session-still-being-closed because the row writes are idempotent
    // and the in-memory Player gets cleaned up on the next world tick).
    //
    // Destructive: requires explicit "confirm" arg. Pool ACCOUNTS are
    // kept (BotAccountMgr is told each character was removed so the
    // per-account character count drops, and the pool reuses these
    // accounts for the fresh wave of spawns).
    static bool HandleWipeFleet(ChatHandler* handler, Optional<std::string> confirm_arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return true; }

        if (!confirm_arg || *confirm_arg != "confirm")
        {
            handler->SendSysMessage(
                "WipeFleet: hard-deletes ALL V2 bot characters and respawns from scratch. "
                "Pool accounts are kept. This is the right command after pipeline fixes "
                "that need fresh characters to take effect (vs `reseed` which only re-runs "
                "the pipeline on existing chars). Run `.playerbot wipefleet confirm`.");
            return true;
        }

        // Snapshot every V2 character_guid + its account before mutating
        // anything. Reading the row first lets us free the pool-account
        // slot via BotAccountMgr after the character is gone.
        auto res = CharacterDatabase.PQuery(
            "SELECT pv.character_guid_low, c.account FROM playerbot_v2_character pv "
            "JOIN characters c ON c.guid = pv.character_guid_low");
        if (!res || !res->GetRowCount())
        {
            handler->SendSysMessage("WipeFleet: no V2 bots to delete.");
            return true;
        }

        uint32 logged_out = 0;
        uint32 deleted    = 0;
        uint32 skipped    = 0;
        do
        {
            Field* f = res->Fetch();
            uint64 guid_low = f[0].GetUInt64();
            uint32 account  = f[1].GetUInt32();
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(guid_low);

            // Online: kick first. Server processes the logout asynchronously
            // but the row deletion below is independent of in-memory state.
            if (ObjectAccessor::FindConnectedPlayer(g))
            {
                Playerbot::Services::SessionMgr().LogoutBot(g);
                ++logged_out;
            }

            // Mirror HandleDelete's unmark + unregister sequence so the
            // V2 services don't keep dangling references to the soon-to-be
            // deleted character.
            if (Playerbot::Services::Registry().has(guid_low))
            {
                Playerbot::Services::Scheduler().unregister_bot(guid_low);
                Playerbot::Services::Snapshots().remove(guid_low);
                Playerbot::Services::Registry().unregister_bot(guid_low);
            }
            Playerbot::Services::Lifecycle().unmark_as_bot(guid_low);

            // Hard delete (deleteFinally=true bypasses the soft-delete pool).
            // Skip if the character is somehow gone already (shouldn't happen
            // since we just queried the row, but defensive).
            if (sCharacterCache->GetCharacterAccountIdByGuid(g) != 0 ||
                CharacterDatabase.PQuery("SELECT 1 FROM characters WHERE guid={}", guid_low))
            {
                Player::DeleteFromDB(g, account, /*updateRealmChars*/true,
                                                 /*deleteFinally*/true);
                Playerbot::Services::Accounts().note_character_removed(account);
                ++deleted;
            }
            else
            {
                ++skipped;
            }
        } while (res->NextRow());

        // Drop residual playerbot_v2_character rows in one shot. Some may
        // already be gone if Player::DeleteFromDB cascaded into our table
        // (it doesn't, currently â but harmless to run unconditionally).
        CharacterDatabase.DirectExecute(
            "DELETE FROM playerbot_v2_character");

        // Seed the fleet with a burst of immediate spawns so the rebuild is
        // visible right away. The default per-cycle throttle (5 spawns / 60s)
        // is appropriate for steady-state shaping but feels like "nothing
        // happens" right after a wipe. We loop ForceReconcile() up to 10
        // times - each call resets budgets to 5 and runs Reconcile, so we
        // get ~50 fresh spawns on the spot. After the burst, the regular
        // 60s reconcile takes over and fills toward the configured target
        // at the steady rate.
        auto& pop = Playerbot::Services::Population();
        constexpr int kBurstCycles = 10;
        for (int i = 0; i < kBurstCycles; ++i)
            pop.ForceReconcile();

        handler->PSendSysMessage(
            "WipeFleet: kicked %u online, deleted %u characters, skipped %u missing. "
            "Burst-spawned ~%d new bots; reconcile continues at %u/cycle until target met.",
            logged_out, deleted, skipped,
            kBurstCycles * 5,
            5u);
        return true;
    }

    // .playerbot graveyardscan [map]
    // Diagnostic, read-only. Every world_safe_locs row (retail data under Classic terrain) is checked against the terrain grid and the
    // static surfaces at its x,y: BURIED = more than a yard under the terrain with no surface near it (a ghost sent there falls out of
    // the world, Karrz 2026-10-10 Sentinel Hill / 2026-10-08 WSG), FLOATING = more than 8 yards above every surface. Prints a ready
    // UPDATE per row (to the caller and to Server.log as "[graveyardscan]"); nothing is written to the database. Rows of maps without a
    // terrain grid (instances, battlegrounds) are only counted.
    static bool HandleGraveyardScan(ChatHandler* handler, Optional<uint32> onlyMap)
    {
        PhaseShift const& phase = PhasingHandler::GetEmptyPhaseShift();
        uint32 scanned = 0, ok = 0, buried = 0, floating = 0, noGrid = 0;
        std::vector<WorldSafeLocsEntry const*> rows;
        for (auto const& [id, entry] : sObjectMgr->GetWorldSafeLocs())
            rows.push_back(&entry);
        std::sort(rows.begin(), rows.end(), [](WorldSafeLocsEntry const* a, WorldSafeLocsEntry const* b) { return a->ID < b->ID; });

        auto say = [&](std::string const& line)
        {
            handler->SendSysMessage(line);
            TC_LOG_INFO("misc", "[graveyardscan] {}", line);
        };

        for (WorldSafeLocsEntry const* row : rows)
        {
            if (row->TransportSpawnId || (onlyMap && *onlyMap != row->Loc.GetMapId()))
                continue;

            ++scanned;
            uint32 const mapId = row->Loc.GetMapId();
            float const x = row->Loc.GetPositionX();
            float const y = row->Loc.GetPositionY();
            float const z = row->Loc.GetPositionZ();
            std::shared_ptr<TerrainInfo> terrainInfo = sTerrainMgr.LoadTerrain(mapId);
            float const terrain = terrainInfo ? terrainInfo->GetGridHeight(phase, mapId, x, y) : INVALID_HEIGHT;
            if (terrain <= INVALID_HEIGHT)
            {
                ++noGrid;
                continue;
            }

            float const nearFloor = terrainInfo->GetStaticHeight(phase, mapId, x, y, z + 2.0f, true, 6.0f);
            bool const onSurface = nearFloor > INVALID_HEIGHT && nearFloor >= z - 1.0f && nearFloor <= z + 2.5f;
            if (z < terrain - 1.0f && !onSurface)
            {
                float const floor = terrainInfo->GetStaticHeight(phase, mapId, x, y, terrain + 1.0f, true, 6.0f);
                float const newZ = std::max(floor, terrain) + 0.5f;
                ++buried;
                say(Trinity::StringFormat("BURIED id {} map {} ({:.1f},{:.1f}) z {:.1f} terrain {:.1f}: UPDATE world_safe_locs SET LocZ = {:.2f} WHERE ID = {};",
                    row->ID, mapId, x, y, z, terrain, newZ, row->ID));
            }
            else if (z > terrain + 8.0f && !onSurface)
            {
                float const above = terrainInfo->GetStaticHeight(phase, mapId, x, y, z, true, 60.0f);
                if (above > INVALID_HEIGHT && z - above > 8.0f)
                {
                    ++floating;
                    say(Trinity::StringFormat("FLOATING id {} map {} ({:.1f},{:.1f}) z {:.1f} surface {:.1f}: UPDATE world_safe_locs SET LocZ = {:.2f} WHERE ID = {};",
                        row->ID, mapId, x, y, z, above, above + 0.5f, row->ID));
                }
                else
                    ++ok;
            }
            else
                ++ok;
        }

        say(Trinity::StringFormat("graveyardscan: {} rows scanned, {} ok, {} buried, {} floating, {} on maps without a terrain grid (not judged)",
            scanned, ok, buried, floating, noGrid));
        return true;
    }

    // .playerbot spellaudit
    // Read-only: writes SpellAudit.tsv to the logs folder, one line per legacy spell (id < 1,400,000: the client's own and Forever's 12xxxxx ids) with the numbers this server holds for it:
    // cast time, cooldowns, duration, range, proc chance and charges, stack, levels, and per effect the type, aura, base points, the vanilla
    // classic_spell_value mean when one applies, per level points, misc value, tick period, trigger spell, chain targets and radius.
    // tools/classic_spell_values/compare_spells.py compares the file with the VMaNGOS spell_template and writes the coverage report.
    static bool HandleSpellAudit(ChatHandler* handler)
    {
        namespace Sa = Playerbot::Diagnostics::SpellAudit;
        std::string const path = sLog->GetLogsDir() + "SpellAudit.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        { handler->PSendSysMessage("spellaudit: cannot write %s", path.c_str()); return false; }
        out << Sa::Header() << '\n';
        // SpellAuditRules.tsv: what makes a spell castable and what it costs (power rows of every type, aura states, required auras, equipped
        // item, stances, attributes, cooldown category) plus the attack power / spell power coefficients of each effect.
        std::ofstream rules(sLog->GetLogsDir() + "SpellAuditRules.tsv", std::ios::trunc);
        if (rules)
            rules << "id\tname\tpowers_type:cost:optional:pct:required_aura\tcaster_aura_state\ttarget_aura_state\tcaster_aura_spell\ttarget_aura_spell\tcaster_aura_not\ttarget_aura_not"
                     "\tequipped_class\tequipped_subclass_mask\tstances\tstances_not\tcategory\tcharge_category\tattr0\tattr1\tattr2\tattr3\tattr4\tattr5\tattr6\tattr7\tattr8"
                     "\te1_bonus\te1_bonus_ap\te2_bonus\te2_bonus_ap\te3_bonus\te3_bonus_ap\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        uint32 written = 0;
        for (uint32 id = 1; id < 1400000; ++id)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            if (!info)
                continue;
            Sa::Row r;
            r.id = id;
            r.name = info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            r.school_mask = info->SchoolMask;
            r.cast_ms = info->CastTimeEntry ? info->CastTimeEntry->Base : 0;
            r.recovery_ms = info->RecoveryTime;
            r.category_recovery_ms = info->CategoryRecoveryTime;
            r.duration_ms = info->GetDuration();
            r.range_max = info->GetMaxRange(false);
            r.proc_chance = info->ProcChance;
            r.proc_charges = info->ProcCharges;
            r.stack = info->StackAmount;
            r.max_targets = info->MaxAffectedTargets;
            r.base_level = info->BaseLevel;
            r.max_level = info->MaxLevel;
            r.spell_level = info->SpellLevel;
            for (SpellPowerEntry const* power : info->PowerCosts)
                if (power && power->PowerType == POWER_MANA)
                {
                    r.mana_cost = power->ManaCost;
                    r.mana_cost_pct = power->PowerCostPct;
                    r.power_row_id = power->ID;
                    break;
                }
            r.family_name = info->SpellFamilyName;
            for (uint32 part = 0; part < 4; ++part)
                r.family_flags[part] = info->SpellFamilyFlags[part];
            std::vector<SpellEffectInfo> const& effects = info->GetEffects();
            for (size_t i = 0; i < effects.size() && i < r.effects.size(); ++i)
            {
                SpellEffectInfo const& e = effects[i];
                Sa::EffectRow& row = r.effects[i];
                row.effect = uint32(e.Effect);
                row.aura = uint32(e.ApplyAuraName);
                row.base_points = e.BasePoints;
                row.points_per_level = e.RealPointsPerLevel;
                row.misc = e.MiscValue;
                row.amplitude_ms = e.ApplyAuraPeriod;
                row.trigger_spell = e.TriggerSpell;
                row.chain_targets = e.ChainTargets;
                row.radius_max = e.TargetARadiusEntry ? e.TargetARadiusEntry->RadiusMax : 0.0f;
                row.radius_b_max = e.TargetBRadiusEntry ? e.TargetBRadiusEntry->RadiusMax : 0.0f;
                if (ClassicSpellValue::Value const* v = sSpellMgr->GetClassicSpellValue(id, uint32(i)))
                {
                    row.classic_value = true;
                    row.classic_mean = ClassicSpellValue::Mean(*v);
                }
            }
            out << Sa::FormatLine(r) << '\n';
            if (rules)
            {
                std::string powers;
                for (SpellPowerEntry const* power : info->PowerCosts)
                    if (power)
                        powers += Trinity::StringFormat("{}{}:{}:{}:{:.1f}:{}", powers.empty() ? "" : ",", int32(power->PowerType), power->ManaCost, power->OptionalCost, power->PowerCostPct, power->RequiredAuraSpellID);
                std::string name = r.name;
                for (char& c : name)
                    if (c == '\t' || c == '\n' || c == '\r')
                        c = ' ';
                std::string line = Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:#x}\t{:#x}\t{}\t{}", id, name, powers, info->CasterAuraState, info->TargetAuraState,
                    info->CasterAuraSpell, info->TargetAuraSpell, info->ExcludeCasterAuraSpell, info->ExcludeTargetAuraSpell, info->EquippedItemClass,
                    info->EquippedItemSubClassMask, info->Stances, info->StancesNot, info->CategoryId, info->ChargeCategoryId);
                line += Trinity::StringFormat("\t{:#x}\t{:#x}\t{:#x}\t{:#x}\t{:#x}\t{:#x}\t{:#x}\t{:#x}\t{:#x}", info->Attributes, info->AttributesEx, info->AttributesEx2,
                    info->AttributesEx3, info->AttributesEx4, info->AttributesEx5, info->AttributesEx6, info->AttributesEx7, info->AttributesEx8);
                for (size_t i = 0; i < 3; ++i)
                    line += i < effects.size() ? Trinity::StringFormat("\t{:.4f}\t{:.4f}", effects[i].BonusCoefficient, effects[i].BonusCoefficientFromAP) : std::string("\t0\t0");
                rules << line << '\n';
            }
            ++written;
        }
        handler->PSendSysMessage("spellaudit: %u spells written to %s", written, path.c_str());
        return true;
    }

    // .playerbot spellcheck [@Player] [class id]
    // Read-only, no cast. For every spell a class trainer teaches that is also a vanilla spell (Diagnostics/SpellCheckExpected.inc, generated from the VMaNGOS 1.12
    // spell table by tools/classic_spell_values/gen_spellcheck_expected.py) the vanilla value formula is evaluated at the caster's level and compared with what the
    // core computes for the same effect (SpellEffectInfo::CalcValue, which includes the classic_spell_value rows). One line per effect in SpellCheck.tsv of the logs
    // folder: PASS, WRONG (amount differs), LAYOUT (the client builds the effect from another effect or aura, so the amounts are not comparable) or UNTESTED (the
    // spell or effect is not in this server's spell store). The caster is the GM's own character, or the player named by "@Name" (from the console).
    static bool HandleSpellCheck(ChatHandler* handler, Tail args)
    {
        namespace Sc = Playerbot::Diagnostics::SpellCheck;
        Player* caster = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        uint32 onlyClass = 0;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
        {
            if (!token.empty() && token[0] == '@')
            {
                caster = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (Optional<uint32> id = Trinity::StringTo<uint32>(token))
                onlyClass = *id;
        }
        if (!caster)
        {
            handler->PSendSysMessage("spellcheck: no player to use as the caster (give @Name from the console)");
            return false;
        }
        std::string const path = sLog->GetLogsDir() + "SpellCheck.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        { handler->PSendSysMessage("spellcheck: cannot write %s", path.c_str()); return false; }
        out << "class\tspell\tname\teffect_index\teffect\taura\tverdict\texpected\tgot\tnote\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        int32 const casterLevel = caster->GetLevel();
        uint32 pass = 0, wrong = 0, layout = 0, missing = 0;
        for (Sc::Expected const& e : Sc::kExpected)
        {
            if (onlyClass && e.cls != onlyClass)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(e.spell, DIFFICULTY_NONE);
            std::string verdict, note;
            double expected = 0.0, got = 0.0;
            if (!info || e.effectIndex >= info->GetEffects().size())
            {
                verdict = "UNTESTED";
                note = "spell or effect not in this server's spell store";
                ++missing;
            }
            else
            {
                SpellEffectInfo const& effect = info->GetEffect(SpellEffIndex(e.effectIndex));
                expected = Sc::VanillaRange(e, casterLevel).mean;
                if (uint32(effect.Effect) != uint32(e.effect) || uint32(effect.ApplyAuraName) != uint32(e.aura))
                {
                    verdict = "LAYOUT";
                    note = Trinity::StringFormat("client effect {} aura {} against vanilla effect {} aura {}", uint32(effect.Effect), uint32(effect.ApplyAuraName), uint32(e.effect), uint32(e.aura));
                    ++layout;
                }
                else
                {
                    // the die makes CalcValue random: the mean of many calls
                    for (uint32 i = 0; i < 64; ++i)
                        got += double(effect.CalcValue(caster));
                    got /= 64.0;
                    if (std::fabs(got - expected) <= std::max(1.5, 0.03 * std::fabs(expected)))
                    {
                        verdict = "PASS";
                        ++pass;
                    }
                    else
                    {
                        verdict = "WRONG";
                        ++wrong;
                    }
                }
            }
            std::string name = info && info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.2f}\t{:.2f}\t{}\n", uint32(e.cls), e.spell, name, uint32(e.effectIndex), uint32(e.effect), uint32(e.aura),
                verdict, expected, got, note);
        }
        handler->PSendSysMessage("spellcheck: caster %s level %d: %u PASS, %u WRONG, %u LAYOUT, %u UNTESTED; lines in %s", caster->GetName().c_str(), casterLevel, pass, wrong, layout, missing, path.c_str());
        return true;
    }

    // .playerbot talentwalk <Name> [<Name> ...]
    // One talent walk (API::apply_classic_talents) for each named player in the world, timed: the milliseconds the world thread spends and the result. A bot
    // whose walk is cached or held back by the duty cycle answers in 0 ms with Ok or Locked. 170 froze when about 60 bots logged in and each walk took seconds;
    // run this from the console on freshly logged-in bots (.playerbot login) to see the cost without a client.
    static bool HandleTalentWalk(ChatHandler* handler, Tail args)
    {
        uint32 walked = 0, total_ms = 0, worst_ms = 0;
        for (std::string_view name : Trinity::Tokenize(std::string_view(args), ' ', false))
        {
            Player* p = ObjectAccessor::FindPlayerByName(name);
            if (!p || !p->IsInWorld())
            {
                handler->PSendSysMessage("talentwalk: %s is not in the world", std::string(name).c_str());
                continue;
            }
            uint32 const start = getMSTime();
            Playerbot::Result const r = Playerbot::API(p).apply_classic_talents();
            uint32 const ms = getMSTimeDiff(start, getMSTime());
            ++walked;
            total_ms += ms;
            worst_ms = std::max(worst_ms, ms);
            handler->PSendSysMessage("talentwalk: %s L%u result %u in %u ms", p->GetName().c_str(), uint32(p->GetLevel()), uint32(r), ms);
        }
        handler->PSendSysMessage("talentwalk: %u walked, %u ms in all, worst %u ms", walked, total_ms, worst_ms);
        return walked > 0;
    }

    // .playerbot auracheck [@Player] [class id]
    // For every aura effect of the vanilla spells a class trainer teaches whose amount is a plain number (stat, attack power, armor, crit and hit, speed, healing and
    // damage done, absorb, periodic damage and heal), the caster gets the real aura on itself (Unit::AddAura: the aura effect's own CalculateAmount, with the
    // caster's level), the amount of the effect is read and compared with the lowest and highest roll of the VMaNGOS value (SpellCheckExpected.h), and the aura is removed
    // again in the same call, so no tick or expiry runs. Absorb, periodic damage and periodic heal amounts also carry spell and healing power: they only have to reach
    // the vanilla minimum. Rows in AuraCheck.tsv of the logs folder: PASS, WRONG (amount outside the range, or no aura came up), LAYOUT (the client effect carries
    // another aura type). Use a level 60 character without talents and with no spell power in the gear for clean numbers.
    static bool HandleAuraCheck(ChatHandler* handler, Tail args)
    {
        namespace Sc = Playerbot::Diagnostics::SpellCheck;
        Player* caster = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        uint32 onlyClass = 0;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
        {
            if (!token.empty() && token[0] == '@')
            {
                caster = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (Optional<uint32> id = Trinity::StringTo<uint32>(token))
                onlyClass = *id;
        }
        if (!caster || !caster->IsInWorld())
        {
            handler->PSendSysMessage("auracheck: no player in the world to use as the caster (give @Name from the console)");
            return false;
        }
        std::string const path = sLog->GetLogsDir() + "AuraCheck.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        { handler->PSendSysMessage("auracheck: cannot write %s", path.c_str()); return false; }
        out << "class\tspell\tname\teffect_index\taura\tverdict\tvanilla_min\tvanilla_max\tgot\tnote\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        int32 const casterLevel = caster->GetLevel();
        uint32 pass = 0, wrong = 0, layout = 0, spells = 0;
        uint32 lastSpell = 0;
        Aura* aura = nullptr;
        for (Sc::Expected const& e : Sc::kExpected)
        {
            if (onlyClass && e.cls != onlyClass)
                continue;
            switch (e.aura)
            {
                case 3: case 8: case 13: case 22: case 24: case 29: case 31: case 33: case 34: case 52: case 53: case 54: case 55: case 57: case 69: case 71:
                case 79: case 85: case 87: case 99: case 101: case 107: case 108: case 124: case 134: case 135: case 136: case 137:
                    break;
                default:
                    continue;
            }
            SpellInfo const* info = sSpellMgr->GetSpellInfo(e.spell, DIFFICULTY_NONE);
            if (e.spell != lastSpell)
            {
                if (lastSpell)
                    caster->RemoveAurasDueToSpell(lastSpell);
                lastSpell = e.spell;
                aura = nullptr;
                if (info)
                {
                    caster->RemoveAurasDueToSpell(e.spell);
                    aura = caster->AddAura(e.spell, caster);
                    ++spells;
                }
            }
            std::string verdict, note;
            Sc::Range const range = Sc::VanillaRange(e, casterLevel);
            double const lo = std::min(range.lo, range.hi), hi = std::max(range.lo, range.hi);
            double got = 0.0;
            if (!info)
            {
                verdict = "LAYOUT";
                note = "spell not in this server's spell store";
                ++layout;
            }
            else if (!aura)
            {
                verdict = "WRONG";
                note = "no aura came up on the caster (immune, restricted or removed by a script)";
                ++wrong;
            }
            else if (AuraEffect const* effect = aura->GetEffect(e.effectIndex); !effect)
            {
                verdict = "LAYOUT";
                note = "the client spell has no such aura effect";
                ++layout;
            }
            else if (uint32(effect->GetAuraType()) != uint32(e.aura))
            {
                verdict = "LAYOUT";
                note = Trinity::StringFormat("client aura {} against vanilla aura {}", uint32(effect->GetAuraType()), uint32(e.aura));
                ++layout;
            }
            else
            {
                got = double(effect->GetAmount());
                double const tolerance = std::max(1.0, 0.02 * std::fabs(range.mean));
                bool const lowerBoundOnly = e.aura == 3 || e.aura == 8 || e.aura == 53 || e.aura == 69;
                bool const ok = lowerBoundOnly ? got >= lo - tolerance : (got >= lo - tolerance && got <= hi + tolerance);
                verdict = ok ? "PASS" : "WRONG";
                if (lowerBoundOnly)
                    note = "minimum only (spell power adds)";
                if (ok)
                    ++pass;
                else
                    ++wrong;
            }
            std::string name = info && info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{:.2f}\t{:.2f}\t{:.2f}\t{}\n", uint32(e.cls), e.spell, name, uint32(e.effectIndex), uint32(e.aura), verdict, lo, hi, got, note);
        }
        if (lastSpell)
            caster->RemoveAurasDueToSpell(lastSpell);
        handler->PSendSysMessage("auracheck: caster %s level %d, %u spells: %u PASS, %u WRONG, %u LAYOUT; lines in %s", caster->GetName().c_str(), casterLevel, spells, pass, wrong, layout, path.c_str());
        return true;
    }

    // .playerbot traitcheck [@Player]
    // Every learned spell that came from a trait node: its definition, rank, and for each effect the trait gives a curve for, the curve's value at
    // that rank against what the spell effect (or the live aura) really has. Writes TraitCheck.tsv. A WRONG row is a rank the amounts do not follow.
    static bool HandleTraitCheck(ChatHandler* handler, Tail args)
    {
        Player* p = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
            if (!token.empty() && token[0] == '@')
                p = ObjectAccessor::FindPlayerByName(token.substr(1));
        if (!p || !p->IsInWorld())
        {
            handler->PSendSysMessage("traitcheck: %s (give @Name from the console)", !p ? "no such online player" : "the player is not in the world");
            return false;
        }
        std::string const path = sLog->GetLogsDir() + "TraitCheck.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        { handler->PSendSysMessage("traitcheck: cannot write %s", path.c_str()); return false; }
        out << "spell\tname\tdefinition\trank\teffect\tcurve\toperation\tcurve_value\tlive\tverdict\tnote\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        uint32 traitSpells = 0, rows = 0, ok = 0, wrong = 0;
        // trait ranks the active config holds against the spells that carry them
        uint32 configRanks = 0;
        if (UF::TraitConfig const* cfg = p->GetTraitConfig(p->m_activePlayerData->ActiveCombatTraitConfigID))
            for (UF::TraitEntry const& e : cfg->Entries)
                if (e.Rank + e.GrantedRanks > 0)
                    ++configRanks;
        if (UF::TraitConfig const* cfg = p->GetTraitConfig(p->m_activePlayerData->ActiveCombatTraitConfigID))
            for (UF::TraitEntry const& e : cfg->Entries)
            {
                if (e.Rank + e.GrantedRanks <= 0)
                    continue;
                TraitNodeEntryEntry const* nodeEntry = sTraitNodeEntryStore.LookupEntry(e.TraitNodeEntryID);
                TraitDefinitionEntry const* definition = nodeEntry ? sTraitDefinitionStore.LookupEntry(nodeEntry->TraitDefinitionID) : nullptr;
                if (!definition)
                {
                    out << Trinity::StringFormat("0\t\t0\t{}\t-\t-\t-\t0\t0\tUNLEARNED\tconfig entry {} has no trait definition\n", int32(e.Rank), e.TraitNodeEntryID);
                    continue;
                }
                PlayerSpell const* learned = definition->SpellID ? Trinity::Containers::MapGetValuePtr(p->GetSpellMap(), uint32(definition->SpellID)) : nullptr;
                if (!learned || !learned->Trait)
                {
                    SpellInfo const* di = definition->SpellID ? sSpellMgr->GetSpellInfo(definition->SpellID, DIFFICULTY_NONE) : nullptr;
                    std::string dname = di && di->SpellName ? std::string((*di->SpellName)[locale]) : std::string();
                    out << Trinity::StringFormat("{}\t{}\t{}\t{}\t-\t-\t-\t0\t0\tUNLEARNED\t{}\n", definition->SpellID, dname, nodeEntry->TraitDefinitionID, int32(e.Rank),
                                                 !definition->SpellID ? "the trait definition grants no spell" : !learned ? "the spell is not in the character's spell map" : "the spell is known but carries no trait link");
                }
            }
        for (auto const& [id, ps] : p->GetSpellMap())
        {
            if (ps.state == PLAYERSPELL_REMOVED || !ps.Trait)
                continue;
            ++traitSpells;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            std::vector<TraitDefinitionEffectPointsEntry const*> const* points = TraitMgr::GetTraitDefinitionEffectPointModifiers(ps.Trait->DefinitionId);
            if (!info || !points)
                continue;
            std::string name = info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            Aura const* aura = p->GetAura(id);
            for (TraitDefinitionEffectPointsEntry const* point : *points)
            {
                float const curve = sDB2Manager.GetCurveValueAt(point->CurveID, float(int32(ps.Trait->Rank)));
                double live = 0.0;
                std::string note;
                if (aura && aura->GetEffect(uint8(point->EffectIndex)))
                    live = aura->GetEffect(uint8(point->EffectIndex))->GetAmount();
                else if (point->EffectIndex < 0 || size_t(point->EffectIndex) >= info->GetEffects().size())
                    note = "DATA: trait points name effect index " + std::to_string(point->EffectIndex) + " the spell does not have";
                else
                {
                    live = info->GetEffect(SpellEffIndex(point->EffectIndex)).CalcValue(p);
                    note = aura ? "no aura effect at this index: spell effect value" : "no aura on the character: spell effect value";
                }
                std::string verdict;
                if (point->GetOperationType() == TraitPointsOperationType::Set)
                    verdict = std::fabs(live - curve) <= 0.51 ? "PASS" : "WRONG";
                else
                {
                    verdict = "UNJUDGED";
                    note += (note.empty() ? "" : "; ") + std::string("multiply or none: no base to compare");
                }
                ++rows;
                if (verdict == "PASS")
                    ++ok;
                else if (verdict == "WRONG")
                    ++wrong;
                out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.2f}\t{:.2f}\t{}\t{}\n", id, name, int32(ps.Trait->DefinitionId), int32(ps.Trait->Rank), int32(point->EffectIndex),
                                             int32(point->CurveID), int32(point->OperationType), curve, live, verdict, note);
            }
        }
        handler->PSendSysMessage("traitcheck: %s: %u trait-learned spells for %u config entries with ranks; %u curve rows: %u PASS, %u WRONG; lines in %s", p->GetName().c_str(), traitSpells, configRanks, rows, ok, wrong, path.c_str());
        return true;
    }

    // .playerbot proccheck [@Player] [spell id...]
    // Does a proc talent fire? For each proc spell (the ids given, or every spell of the character with a spell_proc row) the aura is put on the
    // character and the proc row is tried against events built from the character's own spells: the static answer is
    // SpellMgr::CanSpellTriggerProcOnEvent (does the row's family mask and school match any spell the class casts), the dynamic one is 200 events
    // through Unit::ProcSkillsAndAuras, counting how often the triggered spell, a power gain or damage shows up. Writes ProcCheck.tsv.
    static bool HandleProcCheck(ChatHandler* handler, Tail args)
    {
        Player* p = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        std::vector<uint32> ids;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
        {
            if (!token.empty() && token[0] == '@')
            {
                p = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (Optional<uint32> id = Trinity::StringTo<uint32>(token))
                ids.push_back(*id);
        }
        if (!p || !p->IsInWorld() || !p->IsAlive())
        {
            handler->PSendSysMessage("proccheck: %s (give @Name from the console)", !p ? "no such online player: a name that is not logged in is not found" :
                                     !p->IsInWorld() ? "the player is not in the world (loading, teleporting or logged out)" : "the player is dead or a ghost");
            return false;
        }
        if (ids.empty())
            for (auto const& [id, ps] : p->GetSpellMap())
                if (ps.state != PLAYERSPELL_REMOVED && !ps.disabled)
                    if (SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE))
                        if (si->IsPassive() && sSpellMgr->GetSpellProcEntry(si))
                            ids.push_back(id);
        std::string error;
        Creature* d = Playerbot::Diagnostics::Parity::SpawnProcDummy(p, 2.5f, error);   // in melee reach: a creature moved by NearTeleportTo only arrives at the next map update
        if (!d)
        {
            handler->PSendSysMessage("proccheck: no dummy: %s", error.c_str());
            return false;
        }
        std::string const path = sLog->GetLogsDir() + "ProcCheck.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            d->DespawnOrUnsummon();
            handler->PSendSysMessage("proccheck: cannot write %s", path.c_str());
            return false;
        }
        out << "spell\tname\tverdict\tevent_spells\taccepted_by\tevents\tfired\texpected_chance\ttrigger\tnote\trow\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        constexpr int kEvents = 200;
        uint32 pass = 0, bad = 0, other = 0;
        for (uint32 id : ids)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            std::string verdict, note;
            std::string accepted, row;
            uint32 candidateCount = 0, fired = 0, events = 0, trig = 0;
            float expected = 0.f;
            SpellProcEntry const* entry = info ? sSpellMgr->GetSpellProcEntry(info) : nullptr;
            if (!info)
                verdict = "NO_SPELL";
            else if (!entry)
            {
                verdict = "NO_PROC_ROW";
                row = Trinity::StringFormat("spell proc flags 0x{:x}/0x{:x}, passive {}, effects", uint32(info->ProcFlags[0]), uint32(info->ProcFlags[1]), info->IsPassive());
                for (SpellEffectInfo const& e : info->GetEffects())
                    if (e.IsEffect())
                        row += Trinity::StringFormat(" [{}/{} trigger {}]", uint32(e.Effect), uint32(e.ApplyAuraName), e.TriggerSpell);
            }
            else
            {
                row = Trinity::StringFormat("flags 0x{:x}/0x{:x} family {} mask 0x{:x}/0x{:x}/0x{:x}/0x{:x} school 0x{:x} type 0x{:x} phase 0x{:x} hit 0x{:x} attr 0x{:x} chance {} ppm {} cd {} ms charges {}",
                                            uint32(entry->ProcFlags[0]), uint32(entry->ProcFlags[1]), entry->SpellFamilyName, entry->SpellFamilyMask[0], entry->SpellFamilyMask[1],
                                            entry->SpellFamilyMask[2], entry->SpellFamilyMask[3], entry->SchoolMask, uint32(entry->SpellTypeMask), uint32(entry->SpellPhaseMask),
                                            uint32(entry->HitMask), uint32(entry->AttributesMask), entry->Chance, entry->ProcsPerMinute, entry->Cooldown.count(), entry->Charges);
                bool const added = !p->HasAura(id);
                if (added)
                    p->AddAura(id, p);
                if (!p->HasAura(id))
                    verdict = "NO_AURA";
                else
                {
                    for (SpellEffectInfo const& e : info->GetEffects())
                        if (!trig && e.TriggerSpell)
                            trig = e.TriggerSpell;
                    // the amounts the aura really has on this character (a ranked talent's amounts come from its trait node, not the spell row)
                    if (Optional<PlayerSpellTrait> trait = p->GetTraitInfoForSpell(id))
                    {
                        row += Trinity::StringFormat(" | trait definition {} rank {}", int32(trait->DefinitionId), int32(trait->Rank));
                        if (std::vector<TraitDefinitionEffectPointsEntry const*> const* points = TraitMgr::GetTraitDefinitionEffectPointModifiers(trait->DefinitionId))
                            for (TraitDefinitionEffectPointsEntry const* point : *points)
                                row += Trinity::StringFormat(" curve{}:{}={}", int32(point->EffectIndex), int32(point->CurveID), sDB2Manager.GetCurveValueAt(point->CurveID, float(int32(trait->Rank))));
                    }
                    else
                        row += " | no trait info for this spell (amounts are the spell row's)";
                    if (Aura const* live = p->GetAura(id))
                    {
                        row += " | aura amounts";
                        for (AuraEffect const* eff : live->GetAuraEffects())
                            if (eff)
                                row += Trinity::StringFormat(" e{}={}", uint32(eff->GetEffIndex()), eff->GetAmount());
                    }
                    expected = entry->Chance > 0.f ? entry->Chance : float(info->ProcChance);
                    if (expected > 100.f)
                        expected = 100.f;
                    // the spells the class casts that this row's mask covers (one per spell family member, first rank), or one plain event
                    std::vector<SpellInfo const*> candidates;
                    if (entry->SpellFamilyName == 0 && !entry->SpellFamilyMask)
                        candidates.push_back(nullptr);
                    else
                    {
                        std::set<uint32> firstRanks;
                        for (auto const& [sid, ps] : p->GetSpellMap())
                        {
                            if (ps.state == PLAYERSPELL_REMOVED || ps.disabled || candidates.size() >= 6)
                                continue;
                            SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE);
                            if (!si || si->IsPassive() || !si->IsAffected(entry->SpellFamilyName, entry->SpellFamilyMask))
                                continue;
                            if (firstRanks.insert(si->GetFirstRankSpell()->Id).second)
                                candidates.push_back(si);
                        }
                    }
                    candidateCount = uint32(candidates.size());
                    bool const taken = bool(entry->ProcFlags & TAKEN_HIT_PROC_FLAG_MASK);
                    ProcFlagsSpellType const spellType = entry->SpellTypeMask ? entry->SpellTypeMask : PROC_SPELL_TYPE_DAMAGE;
                    ProcFlagsSpellPhase const phase = entry->SpellPhaseMask ? entry->SpellPhaseMask : PROC_SPELL_PHASE_HIT;
                    ProcFlagsHit const hit = entry->HitMask ? entry->HitMask : (taken ? PROC_HIT_NORMAL : PROC_HIT_NORMAL);
                    Unit* attacker = taken ? static_cast<Unit*>(d) : static_cast<Unit*>(p);
                    Unit* victim = taken ? static_cast<Unit*>(p) : static_cast<Unit*>(d);
                    SpellInfo const* usable = nullptr;
                    for (SpellInfo const* si : candidates)
                    {
                        SpellSchoolMask const school = si ? si->GetSchoolMask() : (entry->SchoolMask ? SpellSchoolMask(entry->SchoolMask) : SPELL_SCHOOL_MASK_NORMAL);
                        DamageInfo dmg(attacker, victim, 100, si, school, SPELL_DIRECT_DAMAGE, BASE_ATTACK);
                        ProcEventInfo ev(attacker, victim, entry->ProcFlags, spellType, phase, hit, nullptr, &dmg, nullptr);
                        if (sSpellMgr->CanSpellTriggerProcOnEvent(*entry, ev))
                        {
                            accepted += (accepted.empty() ? "" : ",") + (si ? std::to_string(si->Id) : std::string("melee"));
                            if (!usable)
                                usable = si;
                        }
                    }
                    if (accepted.empty())
                        verdict = "STATIC_REJECT";
                    else if (!trig)
                        verdict = "STATIC_OK";
                    else
                    {
                        Powers const power = p->GetPowerType();
                        for (int i = 0; i < kEvents; ++i)
                        {
                            p->RemoveAurasDueToSpell(id);
                            p->AddAura(id, p);
                            p->RemoveAurasDueToSpell(trig);
                            d->RemoveAurasDueToSpell(trig);
                            d->SetFullHealth();
                            uint32 const hp = d->GetHealth();
                            p->SetPower(power, 0);
                            p->SetPower(POWER_COMBO_POINTS, 0);
                            SpellSchoolMask const school = usable ? usable->GetSchoolMask() : (entry->SchoolMask ? SpellSchoolMask(entry->SchoolMask) : SPELL_SCHOOL_MASK_NORMAL);
                            DamageInfo dmg(attacker, victim, 100, usable, school, SPELL_DIRECT_DAMAGE, BASE_ATTACK);
                            Unit::ProcSkillsAndAuras(attacker, victim, taken ? ProcFlagsInit(PROC_FLAG_NONE) : entry->ProcFlags, taken ? entry->ProcFlags : ProcFlagsInit(PROC_FLAG_NONE),
                                                     spellType, phase, hit, nullptr, &dmg, nullptr);
                            ++events;
                            if (p->HasAura(trig) || d->HasAura(trig) || d->GetHealth() < hp || p->GetPower(power) > 0 || p->GetPower(POWER_COMBO_POINTS) > 0)
                                ++fired;
                        }
                        if (entry->ProcsPerMinute > 0.f)
                        {
                            verdict = fired ? "PASS" : "NO_FIRE";
                            note = "procs per minute: rate not compared";
                        }
                        else
                        {
                            double const prob = expected / 100.0;
                            double const sigma = std::sqrt(events * prob * (1.0 - prob));
                            double const diff = std::fabs(double(fired) - events * prob);
                            bool const ok = fired > 0 || prob < 0.02;
                            // a strike or spell that triggered can still miss, dodge or parry against the level 63 dummy: a high chance proc may fall
                            // short by up to 15 % without the proc being at fault
                            bool const shortByMiss = prob >= 0.9 && double(fired) >= 0.85 * events * prob && double(fired) <= events * prob;
                            verdict = !ok ? "NO_FIRE" : (diff > 4.0 * sigma + 2.0 && !shortByMiss ? "RATE_OFF" : "PASS");
                            if (shortByMiss && diff > 4.0 * sigma + 2.0)
                                note += (note.empty() ? "" : "; ") + std::string("fired less often than the chance: the triggered strike can miss, dodge or parry");
                            if (entry->Cooldown.count())
                                note = "internal cooldown reset per event";
                        }
                        // Injected events carry no Spell object, so a proc that reads the cast (a finisher's combo cost) can never fire from them:
                        // cast the spell that the row accepted for real, ten times, five combo points and full power each time.
                        if (verdict == "NO_FIRE" && usable)
                        {
                            uint32 casts = 0, signals = 0;
                            int lastResult = -1;
                            for (int i = 0; i < 10; ++i)
                            {
                                p->GetSpellHistory()->ResetAllCooldowns();
                                // the row's internal cooldown (500 ms) would block back-to-back test casts made in one tick, and resetting it on the live aura
                                // did not take effect: a freshly applied aura starts with no cooldown, as the injected-event loop above already relies on
                                p->RemoveAurasDueToSpell(id);
                                p->AddAura(id, p);
                                p->GetSpellHistory()->CancelGlobalCooldown(usable);   // ResetAllCooldowns leaves the global cooldown: the second cast is NOT_READY (91)
                                p->SetPower(power, p->GetMaxPower(power));
                                p->SetPower(POWER_COMBO_POINTS, 5);
                                p->RemoveAurasDueToSpell(trig);
                                d->RemoveAurasDueToSpell(trig);
                                p->SetFacingToObject(d);
                                int32 const pointsBefore = p->GetPower(POWER_COMBO_POINTS);
                                SpellCastResult const r = p->CastSpell(d, usable->Id, CastSpellExtraArgs(TRIGGERED_NONE));
                                lastResult = int(r);
                                note += Trinity::StringFormat("{}cast {}: result {}, points {} -> {}, talent aura {}, trigger aura on self {} / target {}, spell on target {}", note.empty() ? "" : "; ", i,
                                    int(r), pointsBefore, p->GetPower(POWER_COMBO_POINTS), p->HasAura(id), p->HasAura(trig), d->HasAura(trig), d->HasAura(usable->Id));
                                if (r == SPELL_CAST_OK)
                                    ++casts;
                                if (r == SPELL_CAST_OK && (p->GetPower(POWER_COMBO_POINTS) > 0 || p->HasAura(trig) || d->HasAura(trig)))
                                    ++signals;   // a refused cast leaves the five points set and shows nothing
                            }
                            note += Trinity::StringFormat("{}real casts of {}: {} accepted, {} showed the effect, last result {}", note.empty() ? "" : "; ", usable->Id, casts, signals, lastResult);
                            if (signals)
                                verdict = "PASS";
                        }
                        if (Player const* rogue = p; rogue->GetClass() == CLASS_ROGUE)
                            note += Trinity::StringFormat("{}off-hand weapon: {}", note.empty() ? "" : "; ", rogue->GetWeaponForAttack(OFF_ATTACK, true) ? "yes" : "NO");
                    }
                }
                p->RemoveAurasDueToSpell(trig);
                d->RemoveAurasDueToSpell(trig);
                if (added)
                    p->RemoveAurasDueToSpell(id);
            }
            if (verdict == "PASS" || verdict == "STATIC_OK")
                ++pass;
            else if (verdict == "NO_PROC_ROW" || verdict == "NO_SPELL" || verdict == "NO_AURA")
                ++other;
            else
                ++bad;
            std::string name = info && info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.1f}\t{}\t{}\t{}\n", id, name, verdict, candidateCount, accepted, events, fired, expected, trig, note, row);
        }
        d->DespawnOrUnsummon();
        handler->PSendSysMessage("proccheck: %s, %zu spells: %u PASS/STATIC_OK, %u failing, %u not testable; lines in %s", p->GetName().c_str(), ids.size(), pass, bad, other, path.c_str());
        return true;
    }

    // .playerbot learncheck [@Player]
    // Read-only. `.learn my trainer` teaches every client SkillLineAbility spell of the class family (SpellLevel above 0, valid for the player),
    // whether or not a trainer sells it. This lists the ones the player does not know, with whether any trainer row of the class teaches the spell,
    // so spells that only that command gives (and bots never learn) stand out. Writes LearnCheck.tsv; nothing is learned.
    static bool HandleLearnCheck(ChatHandler* handler, Tail args)
    {
        Player* p = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
            if (!token.empty() && token[0] == '@')
                p = ObjectAccessor::FindPlayerByName(token.substr(1));
        if (!p || !p->IsInWorld())
        {
            handler->PSendSysMessage("learncheck: give @Name of an online player");
            return false;
        }
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(p->GetClass());
        if (!classEntry)
            return false;
        std::unordered_set<uint32> taught;
        for (auto const& [trainerId, trainer] : sObjectMgr->GetTrainers())
            if (trainer.GetClassMask() & p->GetClassMask())
                for (Trainer::Spell const& ts : trainer.GetSpells())
                    taught.insert(ts.SpellId);
        std::string const path = sLog->GetLogsDir() + "LearnCheck.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            handler->PSendSysMessage("learncheck: cannot write %s", path.c_str());
            return false;
        }
        out << "spell\tname\tspell_level\tbase_level\tpassive\ttrainer_teaches\tknown\tskill_line\tgives_ranks_of\twhy_the_free_pass_skips_it\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        uint32 listed = 0, notTrainer = 0, alreadyKnown = 0;
        std::unordered_set<uint32> seen;
        for (SkillLineAbilityEntry const* entry : sSkillLineAbilityStore)
        {
            if (!entry || !seen.insert(entry->Spell).second)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(entry->Spell, DIFFICULTY_NONE);
            if (!info || info->SpellLevel == 0 || info->SpellFamilyName != classEntry->SpellClassSet)
                continue;
            if (!p->IsSpellFitByClassAndRace(info->Id) || !SpellMgr::IsSpellValid(info, p, false))
                continue;
            bool const known = p->HasSpell(info->Id);
            if (known)
            {
                ++alreadyKnown;
                continue;
            }
            bool const byTrainer = taught.count(info->Id) != 0;
            if (!byTrainer)
                ++notTrainer;
            ++listed;
            std::string name = info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            std::string why;
            if (byTrainer)
                for (auto const& [trainerId, trainer] : sObjectMgr->GetTrainers())
                    if (trainer.GetType() != Trainer::Type::Tradeskill && trainer.GetType() != Trainer::Type::Pet)
                        for (Trainer::Spell const& ts : trainer.GetSpells())
                            if (ts.SpellId == info->Id && why.empty())
                                why = trainer.DescribeFreeTeachRefusal(p, info->Id);
            out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", info->Id, name, info->SpellLevel, info->BaseLevel, info->IsPassive(), byTrainer,
                known, entry->SkillLine, sSpellMgr->GetFirstSpellInChain(info->Id), why);
        }
        handler->PSendSysMessage("learncheck: %s (level %u): %u spells the .learn path would add, %u of them taught by no trainer row; %u already known; lines in %s",
            p->GetName().c_str(), uint32(p->GetLevel()), listed, notTrainer, alreadyKnown, path.c_str());
        return true;
    }

    // .playerbot xpprobe <player> <quest id>
    // Gives the quest to an online player (a bot over SOAP), completes it and rewards it the way a turn-in does (Player::RewardQuest), and
    // reports the XP it really gained next to the quest's own value and Rate.XP.Quest, so a rate change (.reload config) can be measured
    // without a client. The quest is taken off the player afterwards (also its rewarded mark), so the probe can run again on the same
    // player. Refuses a max-level player (quest XP turns into money there).
    static bool HandleXpProbe(ChatHandler* handler, std::string name, uint32 questId)
    {
        Player* p = normalizePlayerName(name) ? ObjectAccessor::FindPlayerByName(name) : nullptr;
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!p || !p->IsInWorld() || !quest)
        {
            handler->SendSysMessage(!quest ? "xpprobe: no such quest" : "xpprobe: no such player in the world");
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (p->IsMaxLevel())
        {
            handler->PSendSysMessage("xpprobe: %s is max level, quest XP turns into money there", p->GetName().c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // start from no status, so an earlier run (or the bot's own questing) does not block AddQuest
        auto const forget = [p, questId]()
        {
            if (p->GetQuestStatus(questId) != QUEST_STATUS_NONE)
                p->RemoveActiveQuest(questId, false);
            p->RemoveRewardedQuest(questId);
        };
        forget();

        uint32 const baseXp = quest->XPValue(p);
        float const rate = sWorld->getRate(RATE_XP_QUEST);
        uint32 const expected = p->GetQuestXPReward(quest);
        uint8 const levelBefore = p->GetLevel();
        uint32 const xpBefore = p->GetXP();

        p->AddQuest(quest, nullptr);
        p->CompleteQuest(questId);
        QuestStatus const status = p->GetQuestStatus(questId);
        if (status == QUEST_STATUS_COMPLETE)
            p->RewardQuest(quest, LootItemType::Item, 0, p, false);

        // XP gained across any level-ups: the rest of each level passed, plus what stands in the bar now
        uint64 gained = 0;
        uint8 const levelAfter = p->GetLevel();
        if (levelAfter == levelBefore)
            gained = p->GetXP() >= xpBefore ? p->GetXP() - xpBefore : 0;
        else
        {
            gained = sObjectMgr->GetXPForLevel(levelBefore) - xpBefore;
            for (uint8 l = levelBefore + 1; l < levelAfter; ++l)
                gained += sObjectMgr->GetXPForLevel(l);
            gained += p->GetXP();
        }

        forget();

        handler->PSendSysMessage("xpprobe: %s quest %u '%s' level %u->%u: quest value %u, Rate.XP.Quest %.2f, expected %u, gained %u%s",
            p->GetName().c_str(), questId, quest->GetLogTitle().c_str(), uint32(levelBefore), uint32(levelAfter), baseXp, rate, expected,
            uint32(gained), status == QUEST_STATUS_COMPLETE ? "" : " (quest did not reach complete, not rewarded)");
        return true;
    }

    // .playerbot formprobe <player> <form spell id> [<passive spell id>]
    // Learns the form spell if needed, takes the form (a triggered cast: no cost, no cooldown, no level check) and reports the armor before
    // and in the form and whether the form's passive aura is on the player, then leaves the form again and reports the armor back.
    // Moonkin Form: .playerbot formprobe <druid> 24858 24905 (Bear Form for comparison: 5487 1178).
    static bool HandleFormProbe(ChatHandler* handler, std::string name, uint32 formSpell, Optional<uint32> passiveSpell)
    {
        Player* p = normalizePlayerName(name) ? ObjectAccessor::FindPlayerByName(name) : nullptr;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(formSpell, DIFFICULTY_NONE);
        if (!p || !p->IsInWorld() || !p->IsAlive() || !info || !info->HasAura(SPELL_AURA_MOD_SHAPESHIFT))
        {
            handler->SendSysMessage(!info || !info->HasAura(SPELL_AURA_MOD_SHAPESHIFT) ? "formprobe: not a shapeshift spell"
                : "formprobe: no such living player in the world");
            handler->SetSentErrorMessage(true);
            return false;
        }

        bool const learned = !p->HasSpell(formSpell);
        if (learned)
            p->LearnSpell(formSpell, false);

        p->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        uint32 const armorBefore = p->GetArmor();

        SpellCastResult const result = p->CastSpell(p, formSpell, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        uint32 const armorInForm = p->GetArmor();
        bool const inForm = p->HasAura(formSpell);
        bool const hasPassive = passiveSpell && p->HasAura(*passiveSpell);

        p->RemoveAurasDueToSpell(formSpell);
        uint32 const armorAfter = p->GetArmor();
        bool const passiveGone = !passiveSpell || !p->HasAura(*passiveSpell);

        handler->PSendSysMessage("formprobe: %s level %u spell %u%s cast result %u, in form %s (form %u); armor %u -> %u in form -> %u after; "
            "passive %u %s in form, %s after",
            p->GetName().c_str(), uint32(p->GetLevel()), formSpell, learned ? " (learned now)" : "", uint32(result), inForm ? "yes" : "no",
            uint32(info->GetEffect(EFFECT_0).MiscValue), armorBefore, armorInForm, armorAfter, passiveSpell.value_or(0),
            !passiveSpell ? "not checked" : hasPassive ? "present" : "MISSING", passiveGone ? "gone" : "STILL THERE");
        return true;
    }

    // .playerbot langprobe <english line>
    // The line as a Russian reader gets it from a bot (Util/BotLocaleText), or "unchanged" when no catalog entry matches: checks the
    // catalog of the running build without a Russian client.
    static bool HandleLangProbe(ChatHandler* handler, Tail text)
    {
        std::string const english(text);
        std::string const russian = Playerbot::Locale::Translate(Playerbot::Locale::Lang::Ru, english);
        if (russian == english)
            handler->PSendSysMessage("langprobe: unchanged (%u Russian entries): %s", uint32(Playerbot::Locale::Russian().Size()), english.c_str());
        else
            handler->PSendSysMessage("langprobe: ru: %s", russian.c_str());
        return true;
    }

    // .playerbot campprobe <player> go <map> <x> <y> <z>
    // .playerbot campprobe <player> sit [<quest id>]
    // Camping without a client. "go" moves the player (a bot over SOAP) to a camp. "sit" then, in one go: gives the quest if one is named (a
    // The Great Outdoors quest, taken off again at the end), sits the player down the way the sit key does (WorldSession::UpdateCampfireRest),
    // reports the campfire found, the nearest spell focus of any kind, whether the rest started and whether the quest's sit objective counted;
    // then ends the rest as if its minute ran out and reports the camp buffs it gave, and stands the player up again.
    static bool HandleCampProbe(ChatHandler* handler, std::string name, std::string mode, Optional<float> a1, Optional<float> a2,
        Optional<float> a3, Optional<float> a4)
    {
        static constexpr uint32 SPELL_CAMPFIRE_REST = 1289723;
        static constexpr uint32 SPELL_CAMP_BENEFITS = 1229741;
        static constexpr uint32 BoostedBuffs[] = { 1229451, 1229519, 1230164, 1230587, 1230172, 1229513, 1229718, 1230124, 1230098, 1230653 };

        Player* p = normalizePlayerName(name) ? ObjectAccessor::FindPlayerByName(name) : nullptr;
        if (!p || !p->IsInWorld() || !p->IsAlive())
        {
            handler->SendSysMessage("campprobe: no such living player in the world");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (mode == "go")
        {
            if (!a1 || !a2 || !a3 || !a4)
            {
                handler->SendSysMessage("campprobe: go <map> <x> <y> <z>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            bool const ok = p->TeleportTo(uint32(*a1), *a2, *a3, *a4, p->GetOrientation());
            handler->PSendSysMessage("campprobe: %s teleport to map %u (%.2f, %.2f, %.2f) %s", p->GetName().c_str(), uint32(*a1), *a2, *a3, *a4,
                ok ? "started, run sit in a few seconds" : "REFUSED");
            return true;
        }
        if (mode != "sit")
        {
            handler->SendSysMessage("campprobe: mode is go or sit");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // the quest, from no status, so earlier runs or the bot's own questing do not get in the way
        uint32 const questId = a1 ? uint32(*a1) : 0;
        Quest const* quest = questId ? sObjectMgr->GetQuestTemplate(questId) : nullptr;
        if (questId && !quest)
        {
            handler->SendSysMessage("campprobe: no such quest");
            handler->SetSentErrorMessage(true);
            return false;
        }
        auto const forget = [p, questId]()
        {
            if (p->GetQuestStatus(questId) != QUEST_STATUS_NONE)
                p->RemoveActiveQuest(questId, false);
            p->RemoveRewardedQuest(questId);
        };
        if (quest)
        {
            forget();
            p->AddQuest(quest, nullptr);
        }

        // what is near: the campfire as the rest looks for it, and the nearest spell focus of any kind (what the rest used to look at)
        GameObject* fire = nullptr;
        Trinity::GameObjectFocusCheck focusCheck(p, 4);
        Trinity::GameObjectSearcher<Trinity::GameObjectFocusCheck> fireSearcher(p, fire, focusCheck);
        Cell::VisitGridObjects(p, fireSearcher, 15.0f);
        GameObject* anyFocus = p->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_SPELL_FOCUS, 15.0f);
        handler->PSendSysMessage("campprobe: %s map %u (%.2f, %.2f, %.2f); campfire %s; nearest spell focus %s",
            p->GetName().c_str(), p->GetMapId(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
            fire ? Trinity::StringFormat("{} '{}' spawn {} at {:.1f} yd", fire->GetEntry(), fire->GetGOInfo()->name, fire->GetSpawnId(),
                p->GetDistance(fire)).c_str() : "NONE within 15 yd",
            anyFocus ? Trinity::StringFormat("{} '{}' focus type {} at {:.1f} yd", anyFocus->GetEntry(), anyFocus->GetGOInfo()->name,
                anyFocus->GetGOInfo()->spellFocus.spellFocusType, p->GetDistance(anyFocus)).c_str() : "none within 15 yd");

        // sit as the sit key does
        p->RemoveAurasDueToSpell(SPELL_CAMPFIRE_REST);
        p->SetStandState(UNIT_STAND_STATE_SIT);
        p->GetSession()->UpdateCampfireRest(UNIT_STAND_STATE_SIT);
        bool const resting = p->HasAura(SPELL_CAMPFIRE_REST);

        std::string objectives;
        if (quest)
            for (QuestObjective const& objective : quest->GetObjectives())
                objectives += Trinity::StringFormat(" [{} type {}: {}]", objective.StorageIndex, uint32(objective.Type),
                    p->IsQuestObjectiveComplete(objective.QuestID, objective.ID) ? "done" : "not done");
        handler->PSendSysMessage("campprobe: sat down, rest %u %s%s%s", SPELL_CAMPFIRE_REST, resting ? "STARTED" : "did not start",
            quest ? "; quest objectives:" : "", objectives.c_str());

        // the minute runs out: the rest's own script gives the camp buffs
        if (resting)
        {
            p->RemoveAurasDueToSpell(SPELL_CAMPFIRE_REST, ObjectGuid::Empty, 0, AURA_REMOVE_BY_EXPIRE);
            std::string buffs;
            for (uint32 buff : BoostedBuffs)
                if (p->HasAura(buff))
                    buffs += Trinity::StringFormat(" {}", buff);
            handler->PSendSysMessage("campprobe: rest ran out; camp buffs:%s; Camp Benefits %u %s; next rest %s", buffs.empty() ? " NONE" : buffs.c_str(),
                SPELL_CAMP_BENEFITS, p->HasAura(SPELL_CAMP_BENEFITS) ? "on" : "NOT on", p->HasAura(SPELL_CAMPFIRE_REST) ? "started" : "did not start");
        }

        p->SetStandState(UNIT_STAND_STATE_STAND);
        p->GetSession()->UpdateCampfireRest(UNIT_STAND_STATE_STAND);
        if (quest)
            forget();
        return true;
    }

    // .playerbot spotprobe <player> <x> <y> <z> [<from x> <from y> <from z>]
    // Checks a spawn or landing spot on the player's own map (any player on that map, a bot over SOAP): the floor the server sees under the
    // spot (terrain and buildings, as Map::GetHeight finds it searching down from 2 yards above, and from 30 yards above), a 5 x 5 grid of
    // floors a yard apart around it, whether the spot is in water, and, with a start point, whether a walker finds a path from there.
    static bool HandleSpotProbe(ChatHandler* handler, std::string name, float x, float y, float z, Optional<float> fromX, Optional<float> fromY,
        Optional<float> fromZ)
    {
        Player* p = normalizePlayerName(name) ? ObjectAccessor::FindPlayerByName(name) : nullptr;
        if (!p || !p->IsInWorld())
        {
            handler->SendSysMessage("spotprobe: no such player in the world");
            handler->SetSentErrorMessage(true);
            return false;
        }
        Map* map = p->GetMap();
        PhaseShift const& phaseShift = p->GetPhaseShift();
        // the spot may be far from the player: load its grid (terrain, buildings, navmesh tile) first
        map->LoadGrid(x, y);
        if (fromX && fromY)
            map->LoadGrid(*fromX, *fromY);
        auto const floorText = [](float h) { return h <= INVALID_HEIGHT ? std::string("NONE") : Trinity::StringFormat("{:.2f}", h); };

        float const floorNear = map->GetHeight(phaseShift, x, y, z + 2.0f, true, 50.0f);
        float const floorHigh = map->GetHeight(phaseShift, x, y, z + 30.0f, true, 80.0f);
        handler->PSendSysMessage("spotprobe: map %u spot (%.2f, %.2f, %.2f): floor below spot %s, highest floor (from 30 yd up) %s, %s",
            map->GetId(), x, y, z, floorText(floorNear).c_str(), floorText(floorHigh).c_str(),
            map->IsInWater(phaseShift, x, y, z) ? "IN WATER" : "not in water");

        for (int dy = 2; dy >= -2; --dy)
        {
            std::string row;
            for (int dx = -2; dx <= 2; ++dx)
            {
                float const h = map->GetHeight(phaseShift, x + float(dx), y + float(dy), z + 2.0f, true, 50.0f);
                row += Trinity::StringFormat(" {:>7}", floorText(h));
            }
            handler->PSendSysMessage("spotprobe: y%+d:%s", dy, row.c_str());
        }

        if (fromX && fromY && fromZ)
        {
            PathGenerator path(p);
            bool const found = path.CalculatePath(*fromX, *fromY, *fromZ, x, y, z, false);
            Movement::PointsArray const& points = path.GetPath();
            G3D::Vector3 const end = points.empty() ? G3D::Vector3(*fromX, *fromY, *fromZ) : points.back();
            float const endDist = std::sqrt((end.x - x) * (end.x - x) + (end.y - y) * (end.y - y) + (end.z - z) * (end.z - z));
            handler->PSendSysMessage("spotprobe: path from (%.2f, %.2f, %.2f): %s, type 0x%X, %u points, length %.1f yd, ends %.1f yd from the spot (%.2f, %.2f, %.2f)",
                *fromX, *fromY, *fromZ, found ? "found" : "NOT found", uint32(path.GetPathType()), uint32(points.size()), path.GetPathLength(), endDist,
                end.x, end.y, end.z);
        }
        return true;
    }

    // .playerbot collisionprobe <creature entry> [<player>]
    // The collision height of each model of a creature template: the model's raw CollisionHeight, the height the core uses
    // (CreatureModelDataEntry::GetCollisionHeight, which reads the bounding box when the client marks the height "not set") and the
    // scaled unit height. With a player, every creature of that entry within 60 yards of the player is listed with its live
    // Unit::GetCollisionHeight and whether the player and the creature see each other (Map line of sight, as spells and melee check it).
    static bool HandleCollisionProbe(ChatHandler* handler, uint32 entry, Optional<std::string> name)
    {
        CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(entry);
        if (!tmpl)
        {
            handler->PSendSysMessage("collisionprobe: no creature template %u", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }
        for (CreatureModel const& model : tmpl->Models)
        {
            CreatureDisplayInfoEntry const* display = sCreatureDisplayInfoStore.LookupEntry(model.CreatureDisplayID);
            CreatureModelDataEntry const* modelData = display ? sCreatureModelDataStore.LookupEntry(display->ModelID) : nullptr;
            if (!modelData)
            {
                handler->PSendSysMessage("collisionprobe: %u display %u: no display or model data", entry, model.CreatureDisplayID);
                continue;
            }
            handler->PSendSysMessage("collisionprobe: %u display %u model %u: raw CollisionHeight %.2f, used %.2f, bounds top %.2f, unit height %.2f",
                entry, model.CreatureDisplayID, display->ModelID, modelData->CollisionHeight, modelData->GetCollisionHeight(), modelData->GeoBox[5],
                modelData->GetCollisionHeight() * modelData->ModelScale * display->CreatureModelScale);
        }

        if (!name)
            return true;
        std::string playerName = *name;
        Player* p = normalizePlayerName(playerName) ? ObjectAccessor::FindPlayerByName(playerName) : nullptr;
        if (!p || !p->IsInWorld())
        {
            handler->SendSysMessage("collisionprobe: no such player in the world");
            handler->SetSentErrorMessage(true);
            return false;
        }
        std::list<Creature*> creatures;
        p->GetCreatureListWithEntryInGrid(creatures, entry, 60.0f);
        handler->PSendSysMessage("collisionprobe: %u creature(s) of entry %u within 60 yd of %s", uint32(creatures.size()), entry, p->GetName().c_str());
        for (Creature const* c : creatures)
            handler->PSendSysMessage("collisionprobe: spawn %llu at (%.1f, %.1f, %.1f), %.1f yd: unit height %.2f, line of sight %s", uint64(c->GetSpawnId()),
                c->GetPositionX(), c->GetPositionY(), c->GetPositionZ(), p->GetDistance(c), c->GetCollisionHeight(),
                p->IsWithinLOSInMap(c) ? "yes" : "NO");
        return true;
    }

    // .playerbot casttest [@Player] <spell id> [<spell id>...]
    // Casts each active spell once at a dummy in melee reach (full power, no cooldowns, 0 combo points) and writes CastTest.tsv with what changed:
    // the cast result, the dummy's health, the caster's combo points, and every aura that appeared on either unit. A passive spell that carries spell
    // modifiers (aura 107/108) is not cast: the modifier is read through Player::ApplySpellMod against every spell the player knows that it names.
    // `>Name` names a friendly target (another online player) for positive spells that need one, such as a blessing; the dummy is used otherwise.
    static bool HandleCastTest(ChatHandler* handler, Tail args)
    {
        Player* p = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        Player* friendTarget = nullptr;
        bool friendNamed = false;
        std::vector<uint32> ids;
        for (std::string_view token : Trinity::Tokenize(std::string_view(args), ' ', false))
        {
            if (!token.empty() && token[0] == '@')
            {
                p = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (!token.empty() && token[0] == '>')
            {
                friendNamed = true;
                friendTarget = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (Optional<uint32> id = Trinity::StringTo<uint32>(token))
                ids.push_back(*id);
        }
        if (friendNamed && (!friendTarget || !friendTarget->IsInWorld() || !friendTarget->IsAlive()))
        {
            handler->PSendSysMessage("casttest: the friendly target after '>' is not an online living player");
            return false;
        }
        if (!p || !p->IsInWorld() || !p->IsAlive() || ids.empty())
        {
            handler->PSendSysMessage("casttest: needs @Name of a living player in the world and at least one spell id");
            return false;
        }
        // spells that name a party or raid member (a blessing, a heal) need the target in the caster's group: put the named player in it, no invitation
        if (friendTarget && friendTarget != p && !(p->GetGroup() && p->GetGroup() == friendTarget->GetGroup()))
        {
            Group* group = p->GetGroup();
            if (friendTarget->GetGroup() || friendTarget->GetGroupInvite() || (group && (group->IsFull() || !group->IsLeader(p->GetGUID()))))
            {
                handler->PSendSysMessage("casttest: %s is already in another group, or %s's group is full or not led by %s: leave it first", friendTarget->GetName().c_str(), p->GetName().c_str(), p->GetName().c_str());
                return false;
            }
            if (!group)
            {
                group = new Group();
                if (!group->Create(p))
                {
                    delete group;
                    handler->PSendSysMessage("casttest: cannot create a group for %s", p->GetName().c_str());
                    return false;
                }
                sGroupMgr->AddGroup(group);
            }
            if (!group->AddMember(friendTarget))
            {
                handler->PSendSysMessage("casttest: the group refused %s", friendTarget->GetName().c_str());
                return false;
            }
            group->BroadcastGroupUpdate();
        }
        // a spell on another player needs line of sight and range: stand the friendly target beside the caster (same map only; the test's own placement)
        std::string friendPlacement;
        if (friendTarget && friendTarget != p && friendTarget->GetMap() == p->GetMap())
        {
            // The old fixed spot (2 yards east, same Z) can be inside a wall or below the floor, and a friend that is already close can still stand behind
            // one: line of sight (result 71) is checked from the caster, so try four spots around the caster and keep the first one the caster can see.
            auto const hasLos = [&]() { return friendTarget->GetDistance(p) <= 4.0f && p->IsWithinLOSInMap(friendTarget); };
            for (uint32 attempt = 0; attempt < 4 && !hasLos(); ++attempt)
            {
                Position const spot = p->GetNearPosition(2.0f, attempt * float(M_PI) / 2.0f);
                friendTarget->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), p->GetOrientation());
                // A bot's near teleport only lands when its session acks it, and the world tick does that later: casting now would still see the old
                // position. Close the loop here, the way PlayerbotV2 does each tick.
                if (friendTarget->GetTeleportState() == TeleportState::WaitingForTeleportAck && friendTarget->GetSession()
                    && Playerbot::Services::SessionMgr().IsHeadless(friendTarget->GetGUID()))
                {
                    WorldPacket ack(CMSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
                    ack << friendTarget->GetGUID();
                    ack << int32(0);
                    ack << int32(GameTime::GetGameTimeMS());
                    WorldPackets::Movement::MoveTeleportAck ackPacket(std::move(ack));
                    ackPacket.Read();
                    friendTarget->GetSession()->HandleMoveTeleportAck(ackPacket);
                }
            }
            friendPlacement = Trinity::StringFormat("friend at {:.1f} yd, line of sight {}, teleport state {}", friendTarget->GetDistance(p),
                p->IsWithinLOSInMap(friendTarget) ? "yes" : "NO", int(friendTarget->GetTeleportState()));
        }
        std::string error;
        Creature* d = Playerbot::Diagnostics::Parity::SpawnProcDummy(p, 2.5f, error);
        if (!d)
        {
            handler->PSendSysMessage("casttest: no dummy: %s", error.c_str());
            return false;
        }
        std::string const path = sLog->GetLogsDir() + "CastTest.tsv";
        std::ofstream out(path, std::ios::trunc);
        if (!out)
        {
            d->DespawnOrUnsummon();
            handler->PSendSysMessage("casttest: cannot write %s", path.c_str());
            return false;
        }
        out << "spell\tname\tresult\tdummy_health_delta\tcaster_health_delta\tcombo_before\tcombo_after\tnew_auras_on_caster\tnew_auras_on_dummy\tmodifiers\ttargets\n";
        std::ofstream family(sLog->GetLogsDir() + "CastTestFamily.tsv", std::ios::trunc);
        family << "passive\tfamily\tspell\tname\tclass_mask0\tclass_mask1\tclass_mask2\tclass_mask3\tknown\n";
        LocaleConstant const locale = sWorld->GetDefaultDbcLocale();
        // a mounted caster fails every non-mount spell with SPELL_FAILED_NOT_MOUNTED (88): test from the ground
        if (p->IsMounted())
        {
            p->Dismount();
            p->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }
        auto const auraIds = [](Unit const* u)
        {
            std::set<uint32> set;
            for (auto const& [auraId, app] : u->GetAppliedAuras())
                set.insert(auraId);
            return set;
        };
        auto const describeNew = [&](std::set<uint32> const& before, std::set<uint32> const& after)
        {
            std::string text;
            for (uint32 a : after)
                if (!before.count(a))
                {
                    SpellInfo const* ai = sSpellMgr->GetSpellInfo(a, DIFFICULTY_NONE);
                    text += Trinity::StringFormat("{}{} \"{}\"", text.empty() ? "" : ", ", a, ai && ai->SpellName ? (*ai->SpellName)[locale] : "?");
                }
            return text;
        };
        for (uint32 id : ids)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            std::string name, result = "NO_SPELL", casterAuras, dummyAuras, mods;
            int64 dummyDelta = 0, casterDelta = 0;
            int32 comboBefore = 0, comboAfter = 0;
            if (info)
            {
                name = info->SpellName ? std::string((*info->SpellName)[locale]) : std::string();
                if (info->IsPassive())
                {
                    result = "PASSIVE";
                    // a bot that does not have the talent would measure nothing: learn it for the measurement and drop it again afterwards
                    bool const learnedForTest = !p->HasSpell(id) && !p->GetAura(id);
                    if (learnedForTest)
                    {
                        p->LearnSpell(id, false);
                        mods = "learned for the test; ";
                    }
                    // a modifier only applies through a live aura: say whether the player has it, and with what amount per effect
                    if (Aura const* live = p->GetAura(id))
                    {
                        mods += "aura on the player:";
                        for (AuraEffect const* eff : live->GetAuraEffects())
                            mods += Trinity::StringFormat(" e{}={}", uint32(eff->GetEffIndex()), eff->GetAmount());
                        mods += "; ";
                    }
                    else
                        mods += Trinity::StringFormat("NO aura on the player (spell known: {}); ", p->HasSpell(id));
                    for (SpellEffectInfo const& e : info->GetEffects())
                    {
                        if (e.ApplyAuraName != SPELL_AURA_ADD_FLAT_MODIFIER && e.ApplyAuraName != SPELL_AURA_ADD_PCT_MODIFIER)
                            continue;
                        SpellModOp const op = SpellModOp(e.MiscValue);
                        uint32 inFamily = 0, named = 0;
                        for (auto const& [knownId, ps] : p->GetSpellMap())
                        {
                            SpellInfo const* known = sSpellMgr->GetSpellInfo(knownId, DIFFICULTY_NONE);
                            if (!known || known->IsPassive() || ps.state == PLAYERSPELL_REMOVED || known->SpellFamilyName != info->SpellFamilyName || !known->IsAffected(info->SpellFamilyName, e.SpellClassMask))
                                continue;
                            ++named;
                            double value = e.ApplyAuraName == SPELL_AURA_ADD_PCT_MODIFIER ? 1.0 : 0.0;
                            p->ApplySpellMod(known, op, value);
                            mods += Trinity::StringFormat("{}op {} on {} \"{}\": {}", mods.empty() ? "" : ", ", int32(e.MiscValue), knownId,
                                known->SpellName ? (*known->SpellName)[locale] : "?", value);
                        }
                        for (auto const& [knownId, ps] : p->GetSpellMap())
                            if (SpellInfo const* known = sSpellMgr->GetSpellInfo(knownId, DIFFICULTY_NONE))
                                if (!known->IsPassive() && ps.state != PLAYERSPELL_REMOVED && known->SpellFamilyName == info->SpellFamilyName)
                                    ++inFamily;
                        if (!named)
                        {
                            // every spell of that family the client data knows, with its class flags: the mask the talent names is compared against them by eye
                            for (uint32 candidateId = 1; candidateId < 2000000; ++candidateId)
                                if (SpellInfo const* candidate = sSpellMgr->GetSpellInfo(candidateId, DIFFICULTY_NONE))
                                    if (candidate->SpellFamilyName == info->SpellFamilyName && !candidate->IsPassive() && candidate->SpellName && (*candidate->SpellName)[locale] && *(*candidate->SpellName)[locale])
                                        family << id << '\t' << uint32(info->SpellFamilyName) << '\t' << candidateId << '\t' << (*candidate->SpellName)[locale] << '\t'
                                            << candidate->SpellFamilyFlags[0] << '\t' << candidate->SpellFamilyFlags[1] << '\t' << candidate->SpellFamilyFlags[2] << '\t'
                                            << candidate->SpellFamilyFlags[3] << '\t' << p->HasSpell(candidateId) << '\n';
                            mods += Trinity::StringFormat("{}effect {} aura {} op {} mask {:#x}/{:#x}/{:#x}/{:#x} family {}: names none of the {} active spells of that family the player knows",
                                mods.empty() ? "" : ", ", uint32(e.EffectIndex), uint32(e.ApplyAuraName), int32(e.MiscValue), e.SpellClassMask[0], e.SpellClassMask[1],
                                e.SpellClassMask[2], e.SpellClassMask[3], uint32(info->SpellFamilyName), inFamily);
                        }
                    }
                    if (learnedForTest)
                        p->RemoveSpell(id, false, false);
                }
                else
                {
                    p->GetSpellHistory()->ResetAllCooldowns();
                    p->GetSpellHistory()->CancelGlobalCooldown(info);
                    if (Powers const pw = p->GetPowerType(); pw != POWER_COMBO_POINTS)
                        p->SetPower(pw, p->GetMaxPower(pw));
                    p->SetPower(POWER_COMBO_POINTS, 0);
                    p->SetFacingToObject(d);
                    std::set<uint32> const casterBefore = auraIds(p), dummyBefore = auraIds(d), friendBefore = friendTarget ? auraIds(friendTarget) : std::set<uint32>();
                    int64 const dummyHp = d->GetHealth(), casterHp = p->GetHealth();
                    comboBefore = p->GetPower(POWER_COMBO_POINTS);
                    Unit* const target = friendTarget ? static_cast<Unit*>(friendTarget) :
                        (info->IsPositive() && !info->NeedsExplicitUnitTarget() ? static_cast<Unit*>(p) : static_cast<Unit*>(d));
                    SpellCastResult const r = p->CastSpell(target, id, CastSpellExtraArgs(TRIGGERED_NONE));
                    result = Trinity::StringFormat("{}", int(r));
                    comboAfter = p->GetPower(POWER_COMBO_POINTS);
                    dummyDelta = int64(d->GetHealth()) - dummyHp;
                    casterDelta = int64(p->GetHealth()) - casterHp;
                    casterAuras = describeNew(casterBefore, auraIds(p));
                    dummyAuras = describeNew(dummyBefore, auraIds(d));
                    if (friendTarget && friendTarget != p)
                    {
                        std::string const friendAuras = describeNew(friendBefore, auraIds(friendTarget));
                        casterAuras += Trinity::StringFormat("{}[on {}: {}; {}]", casterAuras.empty() ? "" : " ", friendTarget->GetName(), friendAuras.empty() ? "none" : friendAuras, friendPlacement);
                        if (r == SPELL_CAST_OK && !friendAuras.empty() && d->IsAlive())
                        {
                            // measure what the aura does: a Shadow Bolt (686) hit of 200 from the dummy goes through the normal damage pipeline right now. A real
                            // cast would only schedule the hit (projectile travel time), so the health read straight after it showed no change
                            SpellInfo const* probe = sSpellMgr->GetSpellInfo(686, DIFFICULTY_NONE);
                            int64 const friendHp = friendTarget->GetHealth(), casterHp2 = p->GetHealth();
                            if (probe)
                            {
                                SpellNonMeleeDamage hit(d, friendTarget, probe, SpellCastVisual{}, probe->GetSchoolMask());
                                d->CalculateSpellDamageTaken(&hit, 200, probe, BASE_ATTACK);
                                d->DealSpellDamage(&hit, false);
                            }
                            mods += Trinity::StringFormat("dummy hit {} for 200 shadow: target health {}, caster health {}; ", friendTarget->GetName(),
                                int64(friendTarget->GetHealth()) - friendHp, int64(p->GetHealth()) - casterHp2);
                        }
                    }
                }
            }
            for (char& c : name)
                if (c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            std::string targets;
            if (info)
                for (SpellEffectInfo const& e : info->GetEffects())
                    if (e.IsEffect())
                        targets += Trinity::StringFormat("{}e{} {}/{}", targets.empty() ? "" : ", ", uint32(e.EffectIndex), uint32(e.TargetA.GetTarget()), uint32(e.TargetB.GetTarget()));
            if (friendTarget)
                targets += Trinity::StringFormat("{}same group: {}", targets.empty() ? "" : "; ", p->GetGroup() && p->GetGroup() == friendTarget->GetGroup() ? "yes" : "NO");
            out << Trinity::StringFormat("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", id, name, result, dummyDelta, casterDelta, comboBefore, comboAfter, casterAuras, dummyAuras, mods, targets);
        }
        d->DespawnOrUnsummon();
        handler->PSendSysMessage("casttest: %s, %zu spells; lines in %s", p->GetName().c_str(), ids.size(), path.c_str());
        return true;
    }

    // .playerbot creaturestats [@Player] <entry> [<entry>...]
    // Read-only audit of the final health, mana, armor, damage, swing time and resistances creatures get (client ExpectedStat tables x the stored
    // modifiers). Each entry is summoned next to the GM, read, logged as one [creature_stats] line and despawned; no stat is changed.
    static bool HandleCreatureStats(ChatHandler* handler, Tail entries)
    {
        namespace Cs = Playerbot::Diagnostics::CreatureStats;
        // From a GM in game the creatures are summoned next to that GM. From the console or SOAP there is no session, so they are summoned
        // next to the player named by an "@Name" token, or else the first player in the world.
        Player* gm = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        std::vector<uint32> list;
        // "map <id>[,<id>...]" reads every creature entry spawned on those maps instead; "instances" is every vanilla dungeon and raid map
        std::set<uint32> maps;
        bool mapMode = false;
        for (std::string_view token : Trinity::Tokenize(std::string_view(entries), ' ', false))
        {
            if (!token.empty() && token[0] == '@')
            {
                gm = ObjectAccessor::FindPlayerByName(token.substr(1));
                continue;
            }
            if (token == "map")
            {
                mapMode = true;
                continue;
            }
            if (token == "instances")
            {
                for (uint32 mapId : { 33, 34, 36, 43, 47, 48, 70, 90, 109, 129, 189, 209, 229, 230, 249, 289, 329, 349, 389, 409, 429, 469, 509, 531, 533 })
                    maps.insert(mapId);
                continue;
            }
            for (std::string_view part : Trinity::Tokenize(token, ',', false))
                if (Optional<uint32> id = Trinity::StringTo<uint32>(part))
                {
                    if (mapMode)
                        maps.insert(*id);
                    else
                        list.push_back(*id);
                }
        }
        if (!maps.empty())
        {
            std::set<uint32> distinct(list.begin(), list.end());
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
                if (maps.count(data.mapId))
                    distinct.insert(data.id);
            list.assign(distinct.begin(), distinct.end());
            handler->PSendSysMessage("creaturestats: %u distinct creature entries on %u map(s).", uint32(list.size()), uint32(maps.size()));
        }
        if (!gm)
            for (auto const& [id, session] : sWorld->GetAllSessions())
                if (Player* cand = session ? session->GetPlayer() : nullptr; cand && cand->IsInWorld())
                { gm = cand; break; }
        if (!gm || !gm->IsInWorld())
        { handler->SendSysMessage("creaturestats needs an in-world player to summon next to (name one with @Name)."); return false; }
        if (list.empty())
        { handler->SendSysMessage("Usage: .playerbot creaturestats [@Player] <entry> [<entry>...] | map <mapId>[,<mapId>...] | instances"); return false; }
        std::vector<std::string> report;
        uint32 const logged = Cs::Run(gm, list, report);
        for (std::string const& line : report)
            handler->SendSysMessage(line);
        handler->PSendSysMessage("creaturestats: %u of %u entries logged as [creature_stats] in Playerbot.log.", logged, uint32(list.size()));
        return logged > 0;
    }

    // .playerbot scenario list | run <name>[@checkpoint] | runall [name part] | runbatch [name part] | summary | status | result | abort
    // Bot-only scenario tests (Diagnostics/ScenarioRunner.h): a squad is made, staged, grouped and sent into content with no client
    // logged in, watched by the invariant checks, and judged. Works from the console, so a script can start it and scrape
    // "[scenario] result=" from Playerbot.log.
    static bool HandleScenario(ChatHandler* handler, Optional<std::string> verb, Optional<std::string> name)
    {
        namespace Scn = Playerbot::Diagnostics::Scenario;
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        std::string const v = verb ? *verb : std::string("list");
        auto say = [&](std::string const& text)
        {
            for (auto const& line : Trinity::Tokenize(text, '\n', true))
                handler->SendSysMessage(std::string{line});
        };
        if (v == "list")   { say(Scn::List()); return true; }
        if (v == "status") { say(Scn::Status()); return true; }
        if (v == "result") { say(Scn::LastResult()); return true; }
        if (v == "summary") { say(Scn::Summary()); return true; }
        if (v == "runall")
        {
            std::string error;
            if (!Scn::StartAll(name ? *name : std::string(), error))
            { handler->PSendSysMessage("scenario: %s.", error.c_str()); return false; }
            handler->SendSysMessage("runall started. `.playerbot scenario summary` lists the finished ones, one line each.");
            return true;
        }
        if (v == "runbatch")
        {
            std::string error;
            if (!Scn::StartBatch(name ? *name : std::string(), 5, error))
            { handler->PSendSysMessage("scenario: %s.", error.c_str()); return false; }
            handler->SendSysMessage("runbatch started: five dungeons at a time, the next five when all have passed. `.playerbot scenario summary` lists the finished ones.");
            return true;
        }
        if (v == "abort")
        {
            handler->SendSysMessage(Scn::Abort() ? "Scenario aborted." : "No scenario is running.");
            return true;
        }
        if (v == "cleanup")
        {
            // The test characters no running scenario holds leave the world now (the idle sweep does the same every 30 s).
            std::string names;
            std::uint32_t const n = Scn::LogoutIdleTestBots(&names);
            if (n)
                handler->PSendSysMessage("scenario cleanup: %u test character(s) logged out: %s.", n, names.c_str());
            else
                handler->SendSysMessage("scenario cleanup: no idle test character is online.");
            return true;
        }
        if (v == "run")
        {
            if (!name)
            { handler->SendSysMessage("Usage: .playerbot scenario run <name>"); return false; }
            std::string error;
            if (!Scn::Start(*name, error))
            { handler->PSendSysMessage("scenario: %s.", error.c_str()); return false; }
            handler->PSendSysMessage("Scenario '%s' started. `.playerbot scenario status` shows progress; the verdict is logged as [scenario] result=.", name->c_str());
            return true;
        }
        handler->SendSysMessage("Usage: .playerbot scenario list | run <name>[@checkpoint] | runall [name part] | runbatch [name part] | summary | status | result | abort | cleanup");
        return false;
    }

    // .playerbot smoketest [count] [sub]
    //
    // End-to-end regression harness. With no args (or a numeric count) it
    // launches a fresh run: spawns N bots, waits up to 60s for every bot's
    // setup_pipeline to reach AllDone, then checks pipeline failures, intent
    // history, dropped-intent counter, and DB queue depth. Default count: 5.
    //
    // Sub-commands:
    //   .playerbot smoketest result   â print the most recent finished run
    //   .playerbot smoketest status   â print whether a run is in flight
    //
    // Because the wait phase needs the world thread to keep ticking (so
    // async login + setup-pipeline DB callbacks can fire), the test runs on
    // a detached worker thread and writes the verdict to Server.log under
    // logger "playerbot.v2.smoketest". The command returns immediately;
    // re-run with the "result" sub-command to print the final report once
    // the worker logs PASS/FAIL.
    static bool HandleSmoketest(ChatHandler* handler, Optional<std::string> arg)
    {
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }

        // Sub-command dispatch â string args take precedence over numeric.
        if (arg)
        {
            std::string const& s = *arg;
            if (s == "result" || s == "results")
            {
                auto r = Playerbot::V2::Diagnostics::Snapshot();
                const std::string out = Playerbot::V2::Diagnostics::Render(r);
                for (auto const& line : Trinity::Tokenize(out, '\n', true))
                    handler->SendSysMessage(std::string{line});
                return true;
            }
            if (s == "status")
            {
                handler->PSendSysMessage("Smoketest %s.",
                    Playerbot::V2::Diagnostics::IsRunning() ? "running" : "idle");
                return true;
            }
            // .playerbot smoketest dungeon [<map_id>|all]
            //
            // Static validation pass over the DungeonScript registry: for
            // each (or one) registered script, check that (a) bosses[] is
            // non-empty, (b) every boss creature entry resolves via
            // sObjectMgr->GetCreatureTemplate, (c) every interrupt spell
            // resolves via sSpellMgr->GetSpellInfo. Catches typo'd ids
            // before runtime. No bots spawned. Result printed in-line.
            if (s.starts_with("dungeon"))
            {
                std::string rest = s.size() > 7 ? s.substr(7) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                uint32 only_map = 0;
                if (!rest.empty() && rest != "all")
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest dungeon [<map_id>|all]");
                        return false;
                    }
                    only_map = uint32(n);
                }
                return RunDungeonValidation(handler, only_map);
            }
            // .playerbot smoketest bglive [count]   (Wave C)
            //
            // Live BG-aware smoketest. Spawns N bots (default 10) and waits
            // the standard 60s window â same as `.playerbot smoketest`, but
            // additionally asserts that at least one bot has fired an
            // `idle:bg_*` rule during the window. Requires the BG queue-
            // filler to be enabled in playerbot.conf â without it, bots
            // never queue and the assertion will fail with the canonical
            // "0 of N bots fired any idle:bg_* rule" message.
            //
            // Match this BEFORE the static "bg" sub-command since the prefix
            // "bg" is a substring of "bglive". Without this ordering the
            // static path would swallow the request.
            if (s.starts_with("bglive"))
            {
                std::string rest = s.size() > 6 ? s.substr(6) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                Playerbot::V2::Diagnostics::LaunchOptions opts;
                opts.count = 10;            // larger pool than default 5
                opts.timeout_ms = 60'000;
                opts.sample_bg = true;
                if (!rest.empty())
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest bglive [count]");
                        return false;
                    }
                    opts.count = uint32(n);
                }
                if (Playerbot::V2::Diagnostics::IsRunning())
                {
                    handler->SendSysMessage(
                        "Smoketest already running. Wait for completion or "
                        "use `.playerbot smoketest status`.");
                    return false;
                }
                WorldSession* sess = handler->GetSession();
                if (!sess)
                {
                    handler->SendSysMessage(
                        "Live smoketest requires an in-game GM session.");
                    return false;
                }
                if (!Playerbot::V2::Diagnostics::Launch(
                        sess->GetAccountId(), opts))
                {
                    handler->SendSysMessage("Live smoketest launch refused.");
                    return false;
                }
                handler->PSendSysMessage(
                    "Live BG smoketest started: count=%u timeout=%us. "
                    "Adds an `idle:bg_*` rule-fire assertion to the standard "
                    "pipeline. Result via `.playerbot smoketest result` "
                    "after ~%us.",
                    opts.count, opts.timeout_ms / 1000u, opts.timeout_ms / 1000u);
                return true;
            }
            // .playerbot smoketest bg [<bml_id>|all]
            // Static validation over the BattlegroundScript registry.
            if (s.starts_with("bg"))
            {
                std::string rest = s.size() > 2 ? s.substr(2) : std::string();
                while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
                uint32 only_id = 0;
                if (!rest.empty() && rest != "all")
                {
                    char* end = nullptr;
                    const unsigned long n = std::strtoul(rest.c_str(), &end, 10);
                    if (end == rest.c_str())
                    {
                        handler->SendSysMessage(
                            "Usage: .playerbot smoketest bg [<bml_id>|all]");
                        return false;
                    }
                    only_id = uint32(n);
                }
                return RunBgValidation(handler, only_id);
            }
        }

        if (Playerbot::V2::Diagnostics::IsRunning())
        {
            handler->SendSysMessage(
                "Smoketest already running. Wait for completion or use "
                "`.playerbot smoketest status`.");
            return false;
        }

        // Numeric arg path. Parse the optional count; clamp matches Launch().
        Playerbot::V2::Diagnostics::LaunchOptions opts;
        opts.count = 5;
        if (arg)
        {
            char* end = nullptr;
            const unsigned long n = std::strtoul(arg->c_str(), &end, 10);
            if (end == arg->c_str())
            {
                handler->PSendSysMessage(
                    "Usage: .playerbot smoketest [count] | result | status");
                return false;
            }
            opts.count = uint32(n);
        }

        // BotCharacterFactory needs a WorldSession for locale + audit.
        // Console invocation can't supply one â bail with a friendly hint.
        WorldSession* sess = handler->GetSession();
        if (!sess)
        {
            handler->SendSysMessage(
                "Smoketest requires an in-game GM session "
                "(BotCharacterFactory needs a WorldSession). Run from in-game.");
            return false;
        }

        const bool launched =
            Playerbot::V2::Diagnostics::Launch(sess->GetAccountId(), opts);
        if (!launched)
        {
            handler->SendSysMessage("Smoketest launch refused (already running?).");
            return false;
        }

        handler->PSendSysMessage(
            "Smoketest started: count=%u timeout=%us. Worker thread will log "
            "PASS/FAIL to Server.log (logger 'playerbot.v2.smoketest'). Run "
            "`.playerbot smoketest result` after ~%us for the full report.",
            opts.count, opts.timeout_ms / 1000u, opts.timeout_ms / 1000u);
        return true;
    }

    // .playerbot summon <character_name>
    //
    // Player-facing "summon one of my OWN alts as a bot" command. Distinct
    // from .playerbot mark + .playerbot login (the GM lifecycle pair) in
    // three ways:
    //   1) Requires an in-game session â there's no notion of "your alts"
    //      from console.
    //   2) Enforces account match: the named character must live on the
    //      same account as the issuing player. Without this gate any GM
    //      could conscript anyone's character on the realm.
    //   3) Auto-marks + auto-adopts on success: a single command end-to-end
    //      from "I want to play with my alt" to "bot-controlled bot is
    //      following me." No separate mark/adopt/login dance.
    //
    // Account-wide-progress is the design intent: a player levels their
    // main while their alts (logged in as bots via this command) gain XP
    // from group-completed quests / dungeons. Owner binding scopes whisper
    // commands so the player's other alts can also issue squad commands
    // (player_guid=0 = any character on the account).
    // The rest of the line is taken whole, so a second word reaches the handler and gets its own answer instead of the core's
    // "no detailed usage information" (`.playerbot summon First Last`, Discord 2026-10-24).
    static bool HandleSummon(ChatHandler* handler, Tail args)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); handler->SetSentErrorMessage(true); return false; }
        std::string target_name(args);
        while (!target_name.empty() && (target_name.back() == ' ' || target_name.back() == '\t')) target_name.pop_back();
        while (!target_name.empty() && (target_name.front() == ' ' || target_name.front() == '\t')) target_name.erase(target_name.begin());
        if (target_name.empty())
        {
            handler->SendSysMessage(".playerbot summon <AltName>: logs in one of your offline characters (same account) as a bot.");
            handler->SendSysMessage("Example: .playerbot summon Myalt");
            handler->SetSentErrorMessage(true);
            return false;
        }
        // "Name Surname" as character select shows it, or the words joined; any letter case (TypedCharacterName).
        target_name = TypedCharacterName(target_name);
        if (target_name.empty())
        {
            handler->SendSysMessage(".playerbot summon <AltName>: logs in one of your offline characters (same account) as a bot.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        WorldSession* sess = handler->GetSession();
        if (!sess)
        {
            handler->SendSysMessage(
                "`.playerbot summon` is in-game only (needs your account "
                "context to verify alt ownership).");
            handler->SetSentErrorMessage(true);
            return false;
        }
        Player const* me = sess->GetPlayer();
        if (!me)
        {
            handler->SendSysMessage("No player attached to your session.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        const Playerbot::BotId id = ResolveCharacterId(target_name);
        if (!id)
        {
            handler->PSendSysMessage("No character named '%s' exists.",
                                     target_name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (id == me->GetGUID().GetCounter())
        {
            // Summoning yourself collapses into the Wave-F bot-bot logic-on-self
            // toggle, which is a different command. Refuse explicitly so
            // the user doesn't accidentally hand their main to the bot logic.
            handler->SendSysMessage(
                "That's your current character. Use `.playerbot self on` "
                "to drive your own character with the bot logic instead.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ObjectGuid const target_guid = ObjectGuid::Create<HighGuid::Player>(id);
        const uint32 my_account     = sess->GetAccountId();
        const uint32 target_account = sCharacterCache->GetCharacterAccountIdByGuid(target_guid);
        if (target_account == 0)
        {
            handler->PSendSysMessage("'%s' has no resolvable account.",
                                     target_name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (target_account != my_account)
        {
            // Hard fail with no leak: don't reveal which account owns the
            // target. The player either owns this alt or doesn't â that's
            // all they need to know.
            handler->PSendSysMessage("'%s' is not on your account.",
                                     target_name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        // Refuse if the alt is already logged in as a live player (would
        // cause two sessions for the same GUID). The owner is using that
        // character right now and presumably doesn't want it taken over.
        if (Player const* online = ObjectAccessor::FindConnectedPlayer(target_guid))
        {
            handler->PSendSysMessage("'%s' is currently online (logged in by "
                                     "you or another session).",
                                     online->GetName().c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Per-account summon cap. Default 5 = one regular dungeon group.
        // Counts the account's alts in world as bots right now; a logged-out
        // alt frees its place. The .playerbot self attachment doesn't count
        // against this cap because the player is using their own character.
        const uint8 cap = Playerbot::Services::Config().max_alts_as_bots();
        if (cap == 0)
        {
            handler->SendSysMessage(
                "Alt-summon is disabled on this realm "
                "(PlayerbotV2.MaxAltsAsBots = 0).");
            handler->SetSentErrorMessage(true);
            return false;
        }
        // Only this account's own characters in world as bots: companions (bot pool accounts) and alts logged out again
        // used to count too, so an owner with five companions could never summon an alt.
        const size_t active_alts = Playerbot::Services::SessionMgr().HeadlessCountOnAccount(my_account);
        if (active_alts >= cap)
        {
            handler->PSendSysMessage(
                "Alt-summon cap reached: you already have %zu of %u alts "
                "active as bots. Logout one (`.playerbot self off` or "
                "`/w <alt> logout`) before summoning another.",
                active_alts, unsigned(cap));
            handler->SetSentErrorMessage(true);
            return false;
        }

        // Auto-mark + auto-adopt in one shot. The bot character record is
        // shared with the regular V2 bot pool, but ownership is bound to
        // the summoning player's account so squad/whisper commands route
        // back to them rather than to any GM who happens to be online.
        if (!Playerbot::Services::Lifecycle().is_bot(id))
        {
            Playerbot::Services::Lifecycle().mark_as_bot(id);
        }
        Playerbot::Services::Owners().SetOwner(id, my_account, /*player_guid=*/0u);
        // Brought to the caller and grouped once in world, like `.bot call`; its level, spells, talents and gear stay its own
        // (FinishCompanionSetup, IsPlayersOwnCharacter).
        Playerbot::Services::Population().QueueCompanionSetup(id, me->GetGUID().GetCounter(), uint8(Playerbot::Role::Unknown));

        // Headless login. The world thread will process the async DB
        // callback that finishes the spawn; the bot enters world on the
        // next tick after that callback fires.
        auto res = Playerbot::Services::SessionMgr().LoginBot(target_guid);
        if (!res.ok)
        {
            handler->PSendSysMessage("Summon failed: %s", res.reason.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage(
            "Summoning your alt '%s': it logs in as a bot, comes to you and joins your group. "
            "You (and any other char on your account) can whisper `follow`, `stop`, `engage`, `squad`, `role` to it.",
            target_name.c_str());
        return true;
    }

    // .playerbot self on | off | status
    //
    // First-person testing toggle: attach (or detach) the V2 bot logic to the
    // issuing player's OWN character. Lets the player observe bot logic decisions
    // from inside the bot â vastly deeper insight than reading logs after
    // the fact ("why did it stop?", "why didn't it kick?", "where was the
    // pull heuristic going to send me?").
    //
    // Mechanics: this is a thin wrapper over mark/adopt + register/unregister,
    // scoped to "myself" so the user doesn't have to type their own name.
    // The player's client input still flows through the network normally
    // â IntentExecutor and the client compete on movement and ability use,
    // and the player wins when they actively press a key (the bot logic's movement
    // intents will arrive late and overwrite, but only when the client is
    // idle). For pure observation, the player can simply not move and watch.
    //
    // Status sub-form prints whether bot logic is currently attached to self
    // without changing anything, useful for verifying the toggle.
    // `.playerbot lazy`: one press casts the next spell of the warlock rotation on the target (the add-on's "Lazy lock" key runs this
    // line as a secure macro, so it works in combat).
    static bool HandleLazy(ChatHandler* handler)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        WorldSession* sess = handler->GetSession();
        if (!sess || !sess->GetPlayer())
        {
            handler->SendSysMessage("`.playerbot lazy` is in-game only (needs your character).");
            return false;
        }
        std::string const msg = Playerbot::LazyLockPress(sess->GetPlayer());
        if (!msg.empty())
            handler->SendSysMessage(msg.c_str());
        return true;
    }

    static bool HandleSelf(ChatHandler* handler, std::string const& arg)
    {
        Playerbot::BotLanguage::LocalizedHandler localized(handler); handler = localized.Get();   // replies in the player's language
        if (!Playerbot::Services::Initialized())
        { handler->SendSysMessage("PlayerbotV2 not initialized."); return false; }
        WorldSession* sess = handler->GetSession();
        if (!sess || !sess->GetPlayer())
        {
            handler->SendSysMessage(
                "`.playerbot self` is in-game only (needs your character).");
            return false;
        }
        Player* me = sess->GetPlayer();
        const Playerbot::BotId id = me->GetGUID().GetCounter();
        const bool is_bot      = Playerbot::Services::Lifecycle().is_bot(id);
        const bool is_attached = Playerbot::Services::Registry().has(id);

        std::string mode = arg;
        for (char& c : mode) if (c >= 'A' && c <= 'Z') c = char(c + 32);
        if (mode.empty() || mode == "status")
        {
            handler->PSendSysMessage(
                "Self-bot: marked=%s attached=%s (use `on`/`off` to toggle).",
                is_bot ? "yes" : "no",
                is_attached ? "yes" : "no");
            return true;
        }
        if (mode == "on")
        {
            if (is_attached)
            {
                handler->SendSysMessage("Self-bot is already attached.");
                return true;
            }
            // Mark + adopt to self (account-bound) for this session only. A transient mark, as the add-on's self toggle does: the
            // saved mark brought the bot logic back at every login and the player had to turn it off by hand (Session/HumanLogin.h).
            if (!is_bot)
                Playerbot::Services::Lifecycle().mark_as_bot_transient(id);
            Playerbot::Services::Owners().SetOwner(
                id, sess->GetAccountId(), /*player_guid=*/ 0u);
            if (!Playerbot::Services::Registry().has(id))
            {
                Playerbot::BotPersonality personality =
                    Playerbot::Services::Config().random_personality()
                        ? Playerbot::RandomPersonality(Playerbot::SeedForBot(id))
                        : Playerbot::DefaultPersonality();
                Playerbot::Services::Registry().register_bot(
                    id, personality,
                    Playerbot::BotRng{Playerbot::SeedForBot(id)});
                Playerbot::Services::Scheduler().register_bot(
                    id, Playerbot::ActivityTier::Idle);
            }
            handler->SendSysMessage(
                "Self-bot: ON. Bot logic now drives your character. Your client "
                "input still works â pressing movement keys interrupts the "
                "Bot logic's move intents. Use `.playerbot self off` to detach; it also ends when you log out.");
            return true;
        }
        if (mode == "off")
        {
            if (!is_bot && !is_attached)
            {
                handler->SendSysMessage("Self-bot is not attached.");
                return true;
            }
            if (Playerbot::Services::Registry().has(id))
            {
                Playerbot::Services::Scheduler().unregister_bot(id);
                Playerbot::Services::Snapshots().remove(id);
                Playerbot::Services::Registry().unregister_bot(id);
            }
            if (is_bot)
                Playerbot::Services::Lifecycle().unmark_as_bot(id);
            Playerbot::Services::Owners().ClearOwner(id);
            handler->SendSysMessage(
                "Self-bot: OFF. Bot logic detached. You are back to manual control.");
            return true;
        }
        handler->PSendSysMessage("Usage: .playerbot self on | off | status");
        return false;
    }

    // ----------------------------------------------------------------------
    // .playerbot meta â world-metadata editor.
    //
    // Subcommands:
    //   add <kind> [radius]      â capture current pos + zone, insert row
    //   add_at <kind> <m> <x> <y> <z> [radius]  â explicit coords
    //   list [<kind>] [<r>]      â show rows near caller (default r=200y, all kinds)
    //   delete <id>              â remove row by id
    //   reload                   â re-read DB into in-memory cache
    //   help                     â print kinds + usage
    //
    // Kinds: road, crossroad, city, village, hub, danger, vendor, mailbox,
    //        innkeeper, other (see WorldMetadata.h::WorldMetadataKind).
    //
    // The handler is in-world only â capturing the GM's position requires
    // a Player*. Console-only operators use add_at + map coords.
    static bool HandleMeta(ChatHandler* handler, Optional<std::string> argOpt)
    {
        using namespace ::Playerbot::V2::World;

        std::string args = argOpt ? *argOpt : std::string();
        // Trim leading whitespace
        size_t s = 0;
        while (s < args.size() && std::isspace(static_cast<unsigned char>(args[s]))) ++s;
        if (s) args.erase(0, s);

        // Auto-export helper: write the full metadata cache to a flat
        // CSV after every successful add/delete so the next
        // mmaps_generator regen automatically picks up the new
        // waypoints. Default destination matches the generator's
        // auto-discovery path (<data>/world_metadata.csv).
        auto auto_export = [](WorldMetadataStore& s) {
            // Use M:/WorldofWarcraft/world_metadata.csv literally â
            // that's the inputDirectory the GM-curated regens use. If
            // operators want a different path they invoke `meta export
            // <path>` explicitly.
            std::FILE* f = std::fopen("M:/WorldofWarcraft/world_metadata.csv", "wb");
            if (!f) return;
            auto rows = s.Snapshot();
            std::fprintf(f,
                "# playerbot_v2_world_metadata export â schema_version=9\n"
                "# columns: id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes\n");
            for (auto const& r : rows)
            {
                std::fprintf(f, "%llu,%u,%u,%u,%s,%.3f,%.3f,%.3f,%.2f,%s,%s\n",
                    static_cast<unsigned long long>(r.id),
                    r.map_id, r.zone_id, uint32(r.kind), KindToString(r.kind),
                    r.x, r.y, r.z, r.radius,
                    r.label.c_str(), r.notes.c_str());
            }
            std::fclose(f);
        };

        auto print_help = [&]() {
            handler->SendSysMessage("Playerbot meta â world knowledge editor.");
            handler->SendSysMessage("CREATE / MUTATE:");
            handler->SendSysMessage("  add <kind> [radius]            capture current pos");
            handler->SendSysMessage("  add_at <kind> <m> <x> <y> <z>  explicit coords");
            handler->SendSysMessage("  edit <id> <field>=<value>      radius|label|notes");
            handler->SendSysMessage("  delete <id>");
            handler->SendSysMessage("  delete_near <r> [<kind>]       preview bulk delete");
            handler->SendSysMessage("  delete_near_confirm <r> [<k>]  apply bulk delete");
            handler->SendSysMessage("QUERY:");
            handler->SendSysMessage("  list [<kind>] [<radius_yards>] 30 nearest");
            handler->SendSysMessage("  nearest [<kind>]               single closest");
            handler->SendSysMessage("  count                          total + per-kind summary");
            handler->SendSysMessage("  stats                          per-map per-kind breakdown");
            handler->SendSysMessage("  status                         cache/CSV/DB health check");
            handler->SendSysMessage("STATE:");
            handler->SendSysMessage("  reload                         re-read DB + re-export CSV");
            handler->SendSysMessage("  export [<path>]                dump cache to CSV");
            handler->SendSysMessage("Kinds: road, crossroad, city, village, hub,");
            handler->SendSysMessage("       danger, vendor, mailbox, innkeeper, other");
            handler->SendSysMessage("Prefix-match accepted (eg 'ro' -> road, 'vi' -> village).");
            handler->SendSysMessage("Every mutation auto-exports M:/WorldofWarcraft/world_metadata.csv");
            handler->SendSysMessage("for the next mmaps_generator regen.");
        };

        if (args.empty() || args == "help")
        {
            print_help();
            return true;
        }

        if (args == "examples")
        {
            handler->SendSysMessage("=== Common meta workflows ===");
            handler->SendSysMessage("Annotate a road from Shadowglen to Dolanaar:");
            handler->SendSysMessage("  (ride a mount along the road, stop every 10-15y)");
            handler->SendSysMessage("  .playerbot meta add road");
            handler->SendSysMessage("Mark Dolanaar as a hub for low-level bots:");
            handler->SendSysMessage("  (stand in Dolanaar center)");
            handler->SendSysMessage("  .playerbot meta add hub 80");
            handler->SendSysMessage("Annotate Stormwind cathedral district as city:");
            handler->SendSysMessage("  (stand in cathedral square)");
            handler->SendSysMessage("  .playerbot meta add city 250");
            handler->SendSysMessage("Mark a danger zone with a label:");
            handler->SendSysMessage("  .playerbot meta add danger 60");
            handler->SendSysMessage("  .playerbot meta nearest danger   (note the id)");
            handler->SendSysMessage("  .playerbot meta edit <id> label=DeadminesPath");
            handler->SendSysMessage("Verify your edits:");
            handler->SendSysMessage("  .playerbot meta count");
            handler->SendSysMessage("  .playerbot meta nearest road");
            handler->SendSysMessage("  .playerbot meta status");
            handler->SendSysMessage("After annotation, regen mmaps (worldserver stopped):");
            handler->SendSysMessage("  mmaps_generator.exe <mapId> --input <DataDir> --output <DataDir>");
            return true;
        }

        // Tokenize on whitespace.
        std::vector<std::string> tok;
        {
            std::string cur;
            for (char c : args)
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!cur.empty()) { tok.push_back(std::move(cur)); cur.clear(); }
                }
                else cur.push_back(c);
            }
            if (!cur.empty()) tok.push_back(std::move(cur));
        }

        std::string const& sub = tok[0];

        auto& store = WorldMetadataStore::Instance();

        if (sub == "sanity")
        {
            // Run consistency checks on the cached rows. Reports
            // problematic rows but doesn't auto-fix â operator decides.
            //   * radius <= 0 â placeholder bug
            //   * (x,y,z) == (0,0,0) â likely never-populated row
            //   * duplicate (map_id, kind, x, y) within 1y â likely
            //     accidental double-add
            auto rows = store.Snapshot();
            uint32 bad_radius = 0, zero_coords = 0, dupes = 0;
            std::vector<uint64> dupe_ids;
            for (size_t i = 0; i < rows.size(); ++i)
            {
                auto const& r = rows[i];
                if (r.radius <= 0.f) ++bad_radius;
                if (r.x == 0.f && r.y == 0.f && r.z == 0.f) ++zero_coords;
                for (size_t j = i + 1; j < rows.size(); ++j)
                {
                    auto const& r2 = rows[j];
                    if (r2.map_id != r.map_id) continue;
                    if (r2.kind   != r.kind)   continue;
                    const float dx = r2.x - r.x;
                    const float dy = r2.y - r.y;
                    if (dx*dx + dy*dy < 1.0f)
                    {
                        ++dupes;
                        if (dupe_ids.size() < 5)
                            dupe_ids.push_back(r2.id);
                    }
                }
            }
            handler->PSendSysMessage(
                "=== Meta sanity ===\n"
                "  bad_radius (<=0): {}\n"
                "  zero_coords (0,0,0): {}\n"
                "  near-duplicates: {}",
                bad_radius, zero_coords, dupes);
            if (!dupe_ids.empty())
            {
                std::string ids;
                for (size_t i = 0; i < dupe_ids.size(); ++i)
                {
                    if (i) ids += ',';
                    ids += std::to_string(dupe_ids[i]);
                }
                handler->PSendSysMessage("  first 5 dupe ids: {}", ids);
            }
            return true;
        }

        if (sub == "status")
        {
            // Quick health summary: in-memory cache size + on-disk CSV
            // size + DB row count via direct SELECT. Use to sanity-check
            // that the three are in sync.
            handler->PSendSysMessage("=== Meta status ===");
            handler->PSendSysMessage("  in-memory cache: {} rows", store.Size());
            // CSV file size (existence + bytes)
            auto csv_path = "M:/WorldofWarcraft/world_metadata.csv";
            std::FILE* fp = std::fopen(csv_path, "rb");
            if (fp)
            {
                std::fseek(fp, 0, SEEK_END);
                long sz = std::ftell(fp);
                std::fclose(fp);
                handler->PSendSysMessage("  on-disk CSV   : {} bytes ({})",
                                          sz, csv_path);
            }
            else
            {
                handler->PSendSysMessage("  on-disk CSV   : MISSING ({})", csv_path);
            }
            // DB row count via direct SELECT
            QueryResult res = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM playerbot_v2_world_metadata");
            if (res)
            {
                handler->PSendSysMessage("  DB row count  : {}", res->Fetch()[0].GetUInt64());
            }
            else
            {
                handler->SendSysMessage("  DB row count  : (query failed)");
            }
            return true;
        }

        if (sub == "count")
        {
            // Quick aggregate â total + per-kind, no per-map breakdown.
            auto rows = store.Snapshot();
            std::array<uint32, uint32(WorldMetadataKind::Count_)> by_kind{};
            for (auto const& r : rows)
                if (uint32(r.kind) < by_kind.size())
                    ++by_kind[uint32(r.kind)];
            std::string line = fmt::format("Meta: {} total â", rows.size());
            for (uint32 k = 1; k < uint32(WorldMetadataKind::Count_); ++k)
                if (by_kind[k] > 0)
                    line += fmt::format(" {}={}", KindToString(WorldMetadataKind(k)), by_kind[k]);
            handler->SendSysMessage(line);
            return true;
        }

        if (sub == "stats")
        {
            // Per-map per-kind row counts. Useful for "did the operator
            // populate roads in zones X / Y?" pre-flight before a regen.
            auto rows = store.Snapshot();
            std::map<uint32, std::array<uint32, uint32(WorldMetadataKind::Count_)>> by_map;
            for (auto const& r : rows)
            {
                auto& a = by_map[r.map_id];
                if (uint32(r.kind) < a.size()) ++a[uint32(r.kind)];
            }
            handler->PSendSysMessage("Meta stats: total {} row(s) across {} map(s)",
                rows.size(), by_map.size());
            for (auto const& [mid, arr] : by_map)
            {
                std::string line = fmt::format("  map {}:", mid);
                for (uint32 k = 1; k < uint32(WorldMetadataKind::Count_); ++k)
                {
                    if (arr[k] > 0)
                        line += fmt::format(" {}={}", KindToString(WorldMetadataKind(k)), arr[k]);
                }
                handler->SendSysMessage(line);
            }
            return true;
        }

        if (sub == "reload")
        {
            int64 n = store.ReloadFromDb();
            // Re-export the CSV so mmaps_generator sees the same state
            // a direct DB edit may have introduced. Without this an
            // operator who hand-edited the table via SQL and then ran
            // `meta reload` would have a stale CSV on disk and the next
            // regen would tag based on the old data.
            auto_export(store);
            handler->PSendSysMessage("Meta: reloaded {} record(s) from DB (CSV re-exported)", n);
            return true;
        }

        if (sub == "export")
        {
            // Dump entire cache to a flat CSV at the path given as arg 1
            // (default M:\WorldofWarcraft\world_metadata.csv). mmaps_generator
            // reads this file at regen time when --road-overrides is passed.
            std::string out_path = (tok.size() >= 2)
                ? tok[1]
                : std::string("M:/WorldofWarcraft/world_metadata.csv");
            auto rows = store.Snapshot();
            std::FILE* f = std::fopen(out_path.c_str(), "wb");
            if (!f)
            {
                handler->PSendSysMessage("Meta export: cannot open '{}'", out_path);
                return false;
            }
            std::fprintf(f,
                "# playerbot_v2_world_metadata export â schema_version=9\n"
                "# columns: id,map_id,zone_id,kind,kind_name,x,y,z,radius,label,notes\n");
            for (auto const& r : rows)
            {
                std::fprintf(f, "%llu,%u,%u,%u,%s,%.3f,%.3f,%.3f,%.2f,%s,%s\n",
                    static_cast<unsigned long long>(r.id),
                    r.map_id, r.zone_id, uint32(r.kind), KindToString(r.kind),
                    r.x, r.y, r.z, r.radius,
                    r.label.c_str(), r.notes.c_str());
            }
            std::fclose(f);
            handler->PSendSysMessage("Meta: exported {} row(s) to {}",
                                    rows.size(), out_path);
            return true;
        }

        if (sub == "delete")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("Usage: .playerbot meta delete <id>");
                return false;
            }
            const uint64 id = std::strtoull(tok[1].c_str(), nullptr, 10);
            const bool ok = store.Delete(id);
            if (ok) auto_export(store);
            handler->PSendSysMessage("Meta: delete id={} {}", id, ok ? "OK" : "(not found)");
            return ok;
        }

        if (sub == "delete_near")
        {
            // .playerbot meta delete_near <radius_yards> [<kind>]
            // Bulk delete every row within radius of caller, optionally
            // filtered to a single kind. Confirmation: prints how many
            // would match BEFORE deleting; operator can re-run with
            // "delete_near_confirm" to actually delete. Same args.
            // Safer than allowing the chat-typo to nuke 50 road points.
            Player* p = handler->GetPlayer();
            if (!p || tok.size() < 2)
            {
                handler->SendSysMessage("Usage: .playerbot meta delete_near <radius> [<kind>]");
                return false;
            }
            const float radius = float(std::strtod(tok[1].c_str(), nullptr));
            if (radius <= 0.0f) return false;
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 3) kfilter = ParseKind(tok[2]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = radius * radius;
            std::vector<uint64> hit_ids;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                if (dx*dx + dy*dy <= r2) hit_ids.push_back(r.id);
            }
            handler->PSendSysMessage(
                "Meta delete_near: would delete {} row(s) within {:.0f}y (kind={}). "
                "Run `delete_near_confirm <radius> [<kind>]` to apply.",
                hit_ids.size(), radius,
                kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter));
            return true;
        }

        if (sub == "delete_near_confirm")
        {
            Player* p = handler->GetPlayer();
            if (!p || tok.size() < 2) return false;
            const float radius = float(std::strtod(tok[1].c_str(), nullptr));
            if (radius <= 0.0f) return false;
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 3) kfilter = ParseKind(tok[2]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = radius * radius;
            size_t deleted = 0;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                if (dx*dx + dy*dy <= r2)
                    if (store.Delete(r.id)) ++deleted;
            }
            if (deleted) auto_export(store);
            handler->PSendSysMessage("Meta delete_near: deleted {} row(s)", deleted);
            return true;
        }

        if (sub == "edit")
        {
            // .playerbot meta edit <id> <field>=<value> [<field>=<value> ...]
            // field can be radius | label | notes. Position+kind are not
            // editable (delete+readd if needed).
            if (tok.size() < 3)
            {
                handler->SendSysMessage(
                    "Usage: .playerbot meta edit <id> "
                    "<radius=N|label=Foo|notes=Bar>");
                return false;
            }
            const uint64 id = std::strtoull(tok[1].c_str(), nullptr, 10);
            if (id == 0)
            {
                handler->SendSysMessage("Meta edit: bad id");
                return false;
            }
            bool any_change = false;
            for (size_t i = 2; i < tok.size(); ++i)
            {
                std::string const& kv = tok[i];
                auto eq = kv.find('=');
                if (eq == std::string::npos) continue;
                std::string key = kv.substr(0, eq);
                std::string val = kv.substr(eq + 1);
                if (key == "radius")
                {
                    float r = float(std::strtod(val.c_str(), nullptr));
                    if (r > 0.0f && store.UpdateRadius(id, r))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: radius={:.0f}", id, r);
                    }
                }
                else if (key == "label")
                {
                    if (store.UpdateLabel(id, val))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: label='{}'", id, val);
                    }
                }
                else if (key == "notes")
                {
                    if (store.UpdateNotes(id, val))
                    {
                        any_change = true;
                        handler->PSendSysMessage("Meta edit #{}: notes='{}'", id, val);
                    }
                }
                else
                {
                    handler->PSendSysMessage("Meta edit: unknown field '{}'", key);
                }
            }
            if (any_change) auto_export(store);
            return any_change;
        }

        if (sub == "nearest")
        {
            // .playerbot meta nearest [<kind>]
            // Print the single closest metadata point to the GM (any
            // kind if none specified). Useful for "where is my edit?"
            // sanity checks during editing.
            Player* p = handler->GetPlayer();
            if (!p)
            {
                handler->SendSysMessage("Meta nearest: must be in-world.");
                return false;
            }
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            if (tok.size() >= 2)
                kfilter = ParseKind(tok[1]);
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            if (rows.empty())
            {
                handler->PSendSysMessage("Meta nearest: no rows on map {} (kind={})",
                    p->GetMapId(),
                    kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter));
                return true;
            }
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            WorldMetadataRecord const* best = nullptr;
            float best_dsq = std::numeric_limits<float>::infinity();
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                const float d2 = dx*dx + dy*dy;
                if (d2 < best_dsq) { best_dsq = d2; best = &r; }
            }
            if (best)
            {
                handler->PSendSysMessage(
                    "Meta nearest: #{} {} dist={:.1f}y r={:.0f} pos=({:.1f},{:.1f},{:.1f}) {}",
                    best->id, KindToString(best->kind),
                    std::sqrt(best_dsq), best->radius,
                    best->x, best->y, best->z,
                    best->label.empty() ? "" : best->label.c_str());
            }
            return true;
        }

        if (sub == "list")
        {
            Player* p = handler->GetPlayer();
            if (!p)
            {
                handler->SendSysMessage("Meta list: must be in-world (needs a position).");
                return false;
            }
            // Optional kind filter, optional radius
            WorldMetadataKind kfilter = WorldMetadataKind::Unknown;
            float rfilter = 200.0f;
            for (size_t i = 1; i < tok.size(); ++i)
            {
                WorldMetadataKind k = ParseKind(tok[i]);
                if (k != WorldMetadataKind::Unknown) { kfilter = k; continue; }
                float r = 0.0f;
                if (std::sscanf(tok[i].c_str(), "%f", &r) == 1 && r > 0.0f)
                    rfilter = r;
            }
            auto rows = (kfilter == WorldMetadataKind::Unknown)
                ? store.RecordsForMap(p->GetMapId())
                : store.RecordsForMapAndKind(p->GetMapId(), kfilter);
            const float bx = p->GetPositionX();
            const float by = p->GetPositionY();
            const float r2 = rfilter * rfilter;
            std::vector<std::pair<float, WorldMetadataRecord const*>> hits;
            for (auto const& r : rows)
            {
                const float dx = r.x - bx;
                const float dy = r.y - by;
                const float d2 = dx*dx + dy*dy;
                if (d2 <= r2) hits.emplace_back(d2, &r);
            }
            std::sort(hits.begin(), hits.end(),
                [](auto const& a, auto const& b) { return a.first < b.first; });
            handler->PSendSysMessage(
                "Meta list (map={}, radius={:.0f}y, kind={}): {} hit(s)",
                p->GetMapId(), rfilter,
                kfilter == WorldMetadataKind::Unknown ? "any" : KindToString(kfilter),
                hits.size());
            for (size_t i = 0; i < hits.size() && i < 30; ++i)
            {
                auto const* r = hits[i].second;
                handler->PSendSysMessage(
                    "  #{}  {} r={:.0f}  dist={:.0f}y  ({:.1f}, {:.1f}, {:.1f})  {}",
                    r->id, KindToString(r->kind), r->radius,
                    std::sqrt(hits[i].first), r->x, r->y, r->z,
                    r->label.empty() ? "" : r->label.c_str());
            }
            if (hits.size() > 30)
                handler->PSendSysMessage("  ... ({} more)", hits.size() - 30);
            return true;
        }

        if (sub == "add" || sub == "add_at")
        {
            const bool explicit_coords = (sub == "add_at");
            const size_t needTok = explicit_coords ? 6 : 2;   // at least: add_at <k> <m> <x> <y> <z>; add <k>
            if (tok.size() < needTok)
            {
                if (explicit_coords)
                    handler->SendSysMessage("Usage: .playerbot meta add_at <kind> <map> <x> <y> <z> [radius]");
                else
                    handler->SendSysMessage("Usage: .playerbot meta add <kind> [radius]");
                return false;
            }
            WorldMetadataRecord r;
            r.kind = ParseKind(tok[1]);
            if (r.kind == WorldMetadataKind::Unknown)
            {
                handler->PSendSysMessage(
                    "Meta add: unknown kind '{}'. Try: road, crossroad, city, village, "
                    "hub, danger, vendor, mailbox, innkeeper, other.",
                    tok[1]);
                return false;
            }
            if (explicit_coords)
            {
                r.map_id = uint32(std::strtoul(tok[2].c_str(), nullptr, 10));
                r.x = float(std::strtod(tok[3].c_str(), nullptr));
                r.y = float(std::strtod(tok[4].c_str(), nullptr));
                r.z = float(std::strtod(tok[5].c_str(), nullptr));
                if (tok.size() >= 7)
                    r.radius = float(std::strtod(tok[6].c_str(), nullptr));
            }
            else
            {
                Player* p = handler->GetPlayer();
                if (!p)
                {
                    handler->SendSysMessage("Meta add: must be in-world for position capture. Use add_at for explicit coords.");
                    return false;
                }
                r.map_id = p->GetMapId();
                r.zone_id = p->GetZoneId();
                r.x = p->GetPositionX();
                r.y = p->GetPositionY();
                r.z = p->GetPositionZ();
                if (tok.size() >= 3)
                    r.radius = float(std::strtod(tok[2].c_str(), nullptr));
            }
            // Default radii per kind (when caller didn't specify):
            if (r.radius <= 0.0f)
            {
                switch (r.kind)
                {
                    case WorldMetadataKind::Road:      r.radius = 8.0f;   break;
                    case WorldMetadataKind::Crossroad: r.radius = 15.0f;  break;
                    case WorldMetadataKind::City:      r.radius = 200.0f; break;
                    case WorldMetadataKind::Village:   r.radius = 80.0f;  break;
                    case WorldMetadataKind::Hub:       r.radius = 60.0f;  break;
                    case WorldMetadataKind::Danger:    r.radius = 50.0f;  break;
                    default:                           r.radius = 10.0f;  break;
                }
            }
            if (Player* p = handler->GetPlayer())
                r.created_by = p->GetName();
            const bool ok = store.Insert(r);
            if (ok) auto_export(store);
            if (ok)
                handler->PSendSysMessage(
                    "Meta: added #{} kind={} map={} pos=({:.1f},{:.1f},{:.1f}) r={:.0f}",
                    r.id, KindToString(r.kind), r.map_id,
                    r.x, r.y, r.z, r.radius);
            else
                handler->SendSysMessage("Meta add: DB insert failed (see server log).");
            return ok;
        }

        handler->PSendSysMessage("Meta: unknown subcommand '{}'. Run '.playerbot meta help'.", sub);
        return false;
    }
};

// PlayerScript hook bridge for the PlayerbotControl addon. We listen for
// addon-language whispers (LANG_ADDON / LANG_ADDON_LOGGED) and hand the
// body to AddonControl::OnAddonWhisper. The hook only fires for whispers
// the player initiates â the addon sends to its own player name (self-
// whisper), which still flows through this hook before the WhisperAddon
// path attempts delivery.
//
// We intentionally don't try to BLOCK the original whisper path; the
// receiver is the player themselves and TC's WhisperAddon already exits
// silently when the receiver's session hasn't registered the prefix.
// Side-effect-wise the original code is a no-op for self-targeted addon
// whispers we don't ack, so observation-only is fine.
class playerbot_v2_addon_listener : public PlayerScript
{
public:
    playerbot_v2_addon_listener() : PlayerScript("playerbot_v2_addon_listener") {}

    void OnChat(Player* player, uint32 /*type*/, uint32 lang,
                std::string& msg, Player* receiver) override
    {
        if (lang != LANG_ADDON && lang != LANG_ADDON_LOGGED) return;
        // OnChat doesn't carry the prefix â AddonControl filters on body
        // shape ("1|<seqHex>|...|MTYPE|"). Pass empty prefix.
        Playerbot::V2::AddonControl::OnAddonWhisper(
            player, receiver, /*prefix=*/ std::string{}, msg);
    }
};

// Bot speech (ike3 broadcasts): a companion remarks in party chat on quest progress and on notable
// kills (creatures and enemy players). Observation only; BotChatter filters to V2 bots grouped with a real player and pushes the
// line as a ChatIntent, so nothing here touches the bot beyond reading it.
class playerbot_v2_chatter_listener : public PlayerScript
{
public:
    playerbot_v2_chatter_listener() : PlayerScript("playerbot_v2_chatter_listener") {}

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        if (Playerbot::Services::Initialized())
            Playerbot::BotChatter::OnQuestStatusChange(player, questId);
    }

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        if (Playerbot::Services::Initialized())
        {
            Playerbot::BotChatter::OnCreatureKilled(killer, killed);   // a boss's "gg" comes from Module::OnDeath (any killer)
        }
    }

    void OnPVPKill(Player* killer, Player* killed) override
    {
        if (Playerbot::Services::Initialized())
            Playerbot::BotChatter::OnPlayerKilled(killer, killed);
    }
};

bool Playerbot::V2::AddCompanionForOwner(Player* owner, uint8 cls, Playerbot::Role role, std::string& msg)
{
    return playerbot_v2_commandscript::AddCompanionFor(owner, owner->GetSession(), cls, role, 0,
                                                      uint8(urand(GENDER_MALE, GENDER_FEMALE)), "bot", msg);
}

void AddSC_playerbot_v2_commandscript()
{
    new playerbot_v2_commandscript();
    new playerbot_v2_addon_listener();
    new playerbot_v2_chatter_listener();
}
