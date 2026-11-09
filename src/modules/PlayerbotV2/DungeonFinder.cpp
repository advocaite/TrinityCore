// DungeonFinder.cpp: the bot dungeon finder, `.bot dungeon`. A player picks a dungeon (or "any" for their level); the missing
// tank / healer / damage places are filled with their own level-matched companions (the `.bot lfg` fill), and once the group is
// complete the player is put inside the dungeon's door and every bot of theirs is brought after them (the summon path, so the
// door checks and the group's instance hold). `.bot dungeon leave` takes them back to where they queued. The bots stay ordinary
// companions: nothing in how they fight or follow changes. The 1.60 client has no Dungeon Finder window, so this is a command
// (and the add-on's Dungeon button), not the retail LFG queue.

#include "LfgBotFill.h"

#include "Chat.h"
#include "DB2Stores.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include "DungeonFinderPlan.h"
#include "Fleet/BotPopulationManager.h"
#include "Fleet/InstanceEntry.h"
#include "Fleet/OwnerRegistry.h"
#include "Services.h"
#include "Util/ConfigReader.h"

#include <unordered_map>

namespace Playerbot::V2
{

namespace
{
    // How long a queued group waits for its bots (a new companion takes some seconds: created, levelled, geared, grouped).
    constexpr uint32 kFormMs = 90 * IN_MILLISECONDS;
    // How long the arrival of the player after a door teleport is waited for before the bots are no longer brought.
    constexpr uint32 kArriveMs = 30 * IN_MILLISECONDS;

    enum class Phase : uint8 { Forming, Entering, Leaving };

    struct FinderRun
    {
        Phase phase = Phase::Forming;
        uint32 activity = 0;
        uint32 map = 0;                  // the map the player is on their way to (the dungeon, or back out)
        uint32 size = 5;
        WorldLocation door;
        uint32 sinceMs = 0;
        std::string name;
    };

    // Where the player stood when the group went in, and which dungeon it was, for `.bot dungeon leave`.
    struct ReturnSpot
    {
        WorldLocation loc;
        uint32 dungeonMap = 0;
    };

    // keyed by the player's guid counter
    std::unordered_map<uint64, FinderRun> g_runs;
    std::unordered_map<uint64, ReturnSpot> g_returns;
    uint32 g_lastTickMs = 0;

    void Tell(Player* player, std::string const& text)
    {
        if (player && player->GetSession())
            ChatHandler(player->GetSession()).SendSysMessage(text.c_str());
    }

    std::string NameOf(ListedActivity const& act)
    {
        if (FinderDungeonInfo const* info = FinderDungeonFor(act.activity))
            return info->name;
        if (MapEntry const* map = sMapStore.LookupEntry(act.map))
            return map->MapName[sWorld->GetDefaultDbcLocale()];
        return "map " + std::to_string(act.map);
    }

    std::string ShortWord(ListedActivity const& act)
    {
        FinderDungeonInfo const* info = FinderDungeonFor(act.activity);
        if (!info)
            return FinderNormalize(NameOf(act));
        std::string_view const keys = info->keys;
        return std::string(keys.substr(0, keys.find(' ')));
    }

    std::string GapText(LfgRoleGap gap)
    {
        std::string out;
        auto part = [&](uint8 n, char const* what)
        {
            if (!n) return;
            if (!out.empty()) out += ", ";
            out += std::to_string(n) + " " + what;
        };
        part(gap.tank, "tank");
        part(gap.healer, "healer");
        part(gap.dps, "damage");
        return out;
    }

    // Why the player cannot start (or go in) now; empty = they can.
    std::string StateRefusal(Player* player)
    {
        if (!player->IsAlive())
            return "you are dead";
        if (player->IsInCombat())
            return "you are in combat";
        if (player->IsInFlight())
            return "you are on a flight";
        if (player->InBattleground() || (player->GetMap() && player->GetMap()->IsBattlegroundOrArena()))
            return "you are in a battleground";
        if (player->IsBeingTeleported())
            return "you are being teleported";
        return {};
    }

    // The door of the dungeon: the named areatrigger when it leads into the map, else the first entrance the server knows.
    AreaTriggerTeleport const* DoorOf(ListedActivity const& act)
    {
        uint32 const trigger = FinderEntranceTrigger(act.activity, Playerbot::Services::Config().lfg_finder_entrances());
        AreaTriggerTeleport const* door = trigger ? sObjectMgr->GetAreaTrigger(trigger) : nullptr;
        if (door && door->Loc.GetMapId() == act.map)
            return door;
        if (trigger)
            TC_LOG_INFO("playerbot.v2", "[dungeon_finder] entrance {} of activity {} does not lead into map {}: the map's first entrance is used",
                        trigger, act.activity, act.map);
        return sObjectMgr->GetMapEntranceTrigger(act.map);
    }

    uint32 MembersOf(Player const* leader)
    {
        Group const* group = leader->GetGroup();
        return group ? group->GetMembersCount() : 1;
    }

    bool OwnsBot(Player const* owner, Player const* bot)
    {
        return bot && bot->GetSession() && bot->GetSession()->IsBot() && owner->GetSession() &&
               Playerbot::Services::Owners().GetOwner(bot->GetGUID().GetCounter()).account_id == owner->GetSession()->GetAccountId();
    }

    // The player has arrived: each of their bots in the group is brought to them (revived if dead elsewhere, the door checked); the
    // other real players of the group get a summon they can accept. Returns the bots sent.
    uint32 BringGroup(Player* owner, bool summonPlayers, std::string& refused)
    {
        Group* group = owner->GetGroup();
        if (!group)
            return 0;
        std::vector<Player*> members;
        for (GroupReference const& ref : group->GetMembers())
            if (Player* member = ref.GetSource(); member && member != owner)
                members.push_back(member);
        uint32 brought = 0;
        for (Player* member : members)
        {
            if (OwnsBot(owner, member))
            {
                if (std::string const why = Playerbot::Services::Population().BringToOwner(owner, member, "dungeon finder"); why.empty())
                    ++brought;
                else
                    refused += " " + member->GetName() + ": " + why + ".";
            }
            else if (summonPlayers && member->GetSession() && !member->GetSession()->IsBot() && member->IsInWorld() &&
                     member->GetMapId() != owner->GetMapId())
                member->SendSummonRequestFrom(owner);
        }
        return brought;
    }

    // Sends the player through the dungeon's door. False (and `msg` says why) when the door refuses them.
    bool Enter(Player* leader, FinderRun& run, uint32 nowMs, std::string& msg)
    {
        g_returns[leader->GetGUID().GetCounter()] = ReturnSpot{ WorldLocation(leader->GetMapId(), leader->GetPositionX(), leader->GetPositionY(),
            leader->GetPositionZ(), leader->GetOrientation()), run.map };
        if (!leader->TeleportTo(run.door))
        {
            g_returns.erase(leader->GetGUID().GetCounter());
            msg = "Dungeon finder: the door of " + run.name + " did not let you in.";
            return false;
        }
        run.phase = Phase::Entering;
        run.sinceMs = nowMs;
        return true;
    }
}

std::string DungeonFinderList(Player* player)
{
    auto const& dungeons = Playerbot::Services::Config().lfg_bot_groups_activities();
    uint32 const level = player ? player->GetLevel() : 60;
    std::string out = "Dungeons for level " + std::to_string(level) + ":";
    for (size_t i : FinderDungeonsForLevel(dungeons, level))
        out += " " + NameOf(dungeons[i]) + " (" + ShortWord(dungeons[i]) + ", " + std::to_string(dungeons[i].levelMin) + "-" +
               std::to_string(dungeons[i].levelMax) + ").";
    out += " All: ";
    for (size_t i = 0; i < dungeons.size(); ++i)
        out += (i ? ", " : "") + ShortWord(dungeons[i]);
    out += ". Type .bot dungeon <word> [tank|healer|dps], or .bot dungeon any.";
    return out;
}

bool DungeonFinderQueue(Player* leader, std::string const& what, Playerbot::Role ownRole, std::string& msg)
{
    auto const& config = Playerbot::Services::Config();
    if (!leader || !leader->GetSession())
        return false;
    if (!config.lfg_finder_enable())
    { msg = "The dungeon finder is off on this realm (Playerbot.Lfg.Finder.Enable)."; return false; }
    uint64 const key = leader->GetGUID().GetCounter();
    if (auto const itr = g_runs.find(key); itr != g_runs.end() && itr->second.phase == Phase::Forming)
    { msg = "You are already queued for " + itr->second.name + ". .bot dungeon cancel stops it."; return false; }
    if (std::string const why = StateRefusal(leader); !why.empty())
    { msg = "Dungeon finder: not now, " + why + "."; return false; }
    if (leader->GetMap() && leader->GetMap()->IsDungeon())
    { msg = "Dungeon finder: you are already in an instance. .bot dungeon leave takes you out."; return false; }
    if (Group const* group = leader->GetGroup(); group && group->GetLeaderGUID() != leader->GetGUID())
    { msg = "Dungeon finder: only the group leader can queue the group."; return false; }

    auto const& dungeons = config.lfg_bot_groups_activities();
    std::string const word = FinderNormalize(what);
    std::vector<size_t> picks;
    if (word == "any" || word == "random")
    {
        std::vector<size_t> const fit = FinderDungeonsForLevel(dungeons, leader->GetLevel());
        if (!fit.empty())
            picks.push_back(fit[urand(0, uint32(fit.size() - 1))]);
    }
    else
        picks = MatchFinderDungeon(dungeons, what);
    if (picks.empty())
    { msg = "Dungeon finder: no dungeon called \"" + what + "\". " + DungeonFinderList(leader); return false; }
    if (picks.size() > 1)
    {
        msg = "Dungeon finder: which one?";
        for (size_t i : picks)
            msg += " " + NameOf(dungeons[i]) + " (" + ShortWord(dungeons[i]) + ").";
        return false;
    }
    ListedActivity const& act = dungeons[picks.front()];
    std::string const name = NameOf(act);
    AreaTriggerTeleport const* door = DoorOf(act);
    if (!door)
    { msg = "Dungeon finder: no entrance of " + name + " is known on this realm."; return false; }
    // The same check the dungeon's portal makes (level, attunement, a save of another instance): said now rather than after the wait.
    if (TransferAbortParams const denied = Map::PlayerCannotEnter(act.map, leader))
    { msg = "Dungeon finder: " + name + " does not let you in: " + Fleet::TransferAbortName(denied.Reason) + "."; return false; }

    LfgRoleGap gap;
    std::string report;
    uint32 const started = LfgFillToSize(leader, ownRole, act.size, gap, report);
    uint32 const now = getMSTime();
    g_runs[key] = FinderRun{ Phase::Forming, act.activity, act.map, act.size, door->Loc, now, name };
    TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {} (level {}) queued for {} (activity {} map {} entrance loc {}): party of {}, missing {} -> {} bot(s) started.{}",
                leader->GetName(), leader->GetLevel(), name, act.activity, act.map, door->ID, act.size, GapText(gap), started, report);
    msg = "Dungeon finder: " + name + " (levels " + std::to_string(act.levelMin) + "-" + std::to_string(act.levelMax) + ").";
    if (gap.Total())
        msg += " " + std::to_string(started) + " bot(s) are joining (" + GapText(gap) + " missing). You go in when the group is complete.";
    else
        msg += " Your group is complete: in you go.";
    if (leader->GetLevel() < act.levelMin)
        msg += " Your bots are your level, below the dungeon's.";
    // a role the class cannot play was not counted (LfgRoleForClass): say so, else the player waits for a tank that is them
    uint8 const asked = ownRole == Playerbot::Role::Tank ? kLfgRoleTank : ownRole == Playerbot::Role::Healer ? kLfgRoleHealer : 0;
    if (asked && !LfgRoleForClass(leader->GetClass(), asked))
        msg += std::string(" Your class cannot ") + (asked == kLfgRoleTank ? "tank" : "heal") + ", so you count as damage and a bot brings the "
             + (asked == kLfgRoleTank ? "tank" : "healer") + ".";
    msg += report;
    return true;
}

DungeonFinderState DungeonFinderStatus(Player* player)
{
    DungeonFinderState st;
    if (!player)
        return st;
    st.formSec = kFormMs / IN_MILLISECONDS;
    st.members = MembersOf(player);
    uint64 const key = player->GetGUID().GetCounter();
    Map const* map = player->GetMap();
    bool const inDungeon = map && map->IsDungeon() && !map->IsBattlegroundOrArena();
    if (inDungeon)
    {
        auto const ret = g_returns.find(key);
        st.canLeave = (ret != g_returns.end() && ret->second.dungeonMap == player->GetMapId()) ||
                      sObjectMgr->GetGoBackTrigger(player->GetMapId()) != nullptr;
        st.phase = "inside";
        st.name = map->GetMapName();
    }
    if (auto const itr = g_runs.find(key); itr != g_runs.end())
    {
        FinderRun const& run = itr->second;
        st.phase = run.phase == Phase::Forming ? "forming" : run.phase == Phase::Entering ? "entering" : "leaving";
        st.name = run.name;
        st.size = run.size;
        st.waitedSec = getMSTimeDiff(run.sinceMs, getMSTime()) / IN_MILLISECONDS;
    }
    return st;
}

bool DungeonFinderCancel(Player* player, std::string& msg)
{
    auto const itr = player ? g_runs.find(player->GetGUID().GetCounter()) : g_runs.end();
    if (itr == g_runs.end() || itr->second.phase != Phase::Forming)
    { msg = "You are not queued for a dungeon."; return false; }
    msg = "Dungeon finder: " + itr->second.name + " cancelled. The bots that joined stay in your group (.bot dismiss sends them off).";
    TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {} cancelled the queue for {}", player->GetName(), itr->second.name);
    g_runs.erase(itr);
    return true;
}

bool DungeonFinderLeave(Player* player, std::string& msg)
{
    if (!player)
        return false;
    Map const* map = player->GetMap();
    if (!map || !map->IsDungeon() || map->IsBattlegroundOrArena())
    { msg = "You are not in a dungeon."; return false; }
    if (!player->IsAlive())
    { msg = "Dungeon finder: release or be resurrected first."; return false; }
    if (player->IsInCombat())
    { msg = "Dungeon finder: not while you are in combat."; return false; }
    uint64 const key = player->GetGUID().GetCounter();
    WorldLocation dest;
    bool known = false;
    if (auto const itr = g_returns.find(key); itr != g_returns.end() && itr->second.dungeonMap == player->GetMapId())
    {
        dest = itr->second.loc;
        known = true;
    }
    else if (AreaTriggerTeleport const* exit = sObjectMgr->GetGoBackTrigger(player->GetMapId()))
    {
        dest = exit->Loc;
        known = true;
    }
    if (!known)
    { msg = "Dungeon finder: no way out of this dungeon is known; walk out or use your hearthstone."; return false; }
    if (!player->TeleportTo(dest))
    { msg = "Dungeon finder: the teleport out was refused."; return false; }
    g_returns.erase(key);
    g_runs[key] = FinderRun{ Phase::Leaving, 0, dest.GetMapId(), 0, dest, getMSTime(), "the way out" };
    TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {} leaves map {} for map {} ({:.0f}, {:.0f})", player->GetName(), map->GetId(),
                dest.GetMapId(), dest.GetPositionX(), dest.GetPositionY());
    msg = "Dungeon finder: out you go. Your bots follow.";
    return true;
}

void DungeonFinderTick(uint32 nowMs)
{
    if (g_runs.empty() || getMSTimeDiff(g_lastTickMs, nowMs) < 1000)
        return;
    g_lastTickMs = nowMs;
    for (auto it = g_runs.begin(); it != g_runs.end();)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(it->first));
        FinderRun& run = it->second;
        if (!player)
        {
            it = g_runs.erase(it);              // logged out
            continue;
        }
        if (!player->IsInWorld())
        {
            ++it;                               // between two maps
            continue;
        }
        uint32 const waited = getMSTimeDiff(run.sinceMs, nowMs);
        if (run.phase == Phase::Forming)
        {
            if (Group const* group = player->GetGroup(); group && group->GetLeaderGUID() != player->GetGUID())
            {
                Tell(player, "Dungeon finder: " + run.name + " cancelled, you are no longer the group leader.");
                it = g_runs.erase(it);
                continue;
            }
            uint32 const members = MembersOf(player);
            FinderFormAction const action = DecideFinderEnter(members, run.size, waited, kFormMs);
            if (action == FinderFormAction::Wait)
            {
                ++it;
                continue;
            }
            if (action == FinderFormAction::GiveUp)
            {
                TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {}: no bot joined in {} s, {} given up", player->GetName(), kFormMs / IN_MILLISECONDS, run.name);
                Tell(player, "Dungeon finder: no bot could join, " + run.name + " given up.");
                it = g_runs.erase(it);
                continue;
            }
            // Ready (or the wait is over): in, unless the player is busy right now (a fight): then the next tick tries again.
            if (!StateRefusal(player).empty())
            {
                ++it;
                continue;
            }
            std::string msg;
            TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {}: group {} of {}, going into {}", player->GetName(), members, run.size, run.name);
            if (!Enter(player, run, nowMs, msg))
            {
                TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {}: the door of {} refused the teleport", player->GetName(), run.name);
                Tell(player, msg);
                it = g_runs.erase(it);
                continue;
            }
            Tell(player, "Dungeon finder: into " + run.name + (members < run.size ? " with " + std::to_string(members) + " of " + std::to_string(run.size) + " (not every bot came in time)." : "."));
            ++it;
            continue;
        }

        // Entering / Leaving: once the player stands on the map, the bots are brought.
        bool const arrived = player->GetMapId() == run.map && !player->IsBeingTeleported();
        if (!arrived)
        {
            if (waited >= kArriveMs)
            {
                TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {} did not arrive on map {} within {} s: bots not brought", player->GetName(), run.map, kArriveMs / IN_MILLISECONDS);
                it = g_runs.erase(it);
                continue;
            }
            ++it;
            continue;
        }
        std::string refused;
        uint32 const brought = BringGroup(player, run.phase == Phase::Entering, refused);
        TC_LOG_INFO("playerbot.v2", "[dungeon_finder] {} arrived on map {} ({}): {} bot(s) brought.{}", player->GetName(), run.map,
                    run.phase == Phase::Entering ? "in" : "out", brought, refused);
        if (!refused.empty())
            Tell(player, "Dungeon finder: not every bot could come." + refused);
        it = g_runs.erase(it);
    }
}

} // namespace Playerbot::V2
