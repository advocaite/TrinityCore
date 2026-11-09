#include "LfgBotFill.h"
#include "Api/BotLanguage.h"

#include "Chat.h"
#include "Common.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Group.h"
#include "Map.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "GameTime.h"
#include "Timer.h"
#include "Util.h"
#include "WorldSession.h"
#include "Bot/BotAI.h"
#include "Bot/BotArchetype.h"
#include "Bot/BotRegistry.h"
#include "Fleet/BotGroupBuilder.h"
#include "Fleet/InstanceGroupCap.h"
#include "Fleet/OwnerRegistry.h"
#include "GroupFinderListings.h"
#include "Services.h"
#include "Session/BotSessionMgr.h"
#include "Util/ConfigReader.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace Playerbot::V2
{

namespace
{
    struct PendingListing
    {
        uint32 deadlineMs = 0;
        uint32 categoryId = 0;
        uint32 partySize = kLfgMinPartySize;      // the dungeon's player limit (5 or 10 or more)
        uint32 nextCheckMs = 0;                   // the real players in range are counted again at this time (every 15 s)
        // Trickle: the places missing when the wait started, how many of each the bots have been started for since, and when
        // the next bot is due.
        bool   stepsReady = false;
        LfgRoleGap initialGap{}, startedGap{};
        uint32 stepIntervalMs = 0, nextStepMs = 0;
    };

    // The places still due a bot: what is missing now, but never more than was missing at the start less the bots started since
    // (those may not have joined the group yet).
    LfgRoleGap DueGap(PendingListing const& p, LfgRoleGap gap)
    {
        if (!p.stepsReady)
            return gap;
        gap.tank   = uint8(std::max(0, std::min<int>(gap.tank,   p.initialGap.tank   - p.startedGap.tank)));
        gap.healer = uint8(std::max(0, std::min<int>(gap.healer, p.initialGap.healer - p.startedGap.healer)));
        gap.dps    = uint8(std::max(0, std::min<int>(gap.dps,    p.initialGap.dps    - p.startedGap.dps)));
        return gap;
    }

    constexpr uint32 kCheckIntervalMs = 15 * IN_MILLISECONDS;

    // keyed by the leader's guid counter
    std::unordered_map<uint64, PendingListing> g_pending;
    // the Group Finder category the bot groups are listed in once a player's own dungeon listing has shown it (0 = not seen yet)
    uint32 g_botGroupCategory = 0;
    // everybody with a posted listing, in any category: a grouped player counts as available only when they have one
    std::unordered_set<uint64> g_listed;

    // Real (non-bot) players online who could still join the leader's group: the leader's faction, within Lfg.LevelRange levels of the
    // leader, not in a full group, and either not grouped or listing their own group.
    uint32 RealPlayersInRange(Player const* leader)
    {
        uint32 const range = Playerbot::Services::Config().lfg_level_range();
        uint32 const level = leader->GetLevel();
        uint32 count = 0;
        for (auto const& [guid, other] : ObjectAccessor::GetPlayers())
        {
            if (!other || other == leader || !other->IsInWorld() || !other->GetSession() || other->GetSession()->IsBot())
                continue;
            if (other->GetTeam() != leader->GetTeam())
                continue;
            uint32 const otherLevel = other->GetLevel();
            if ((otherLevel > level ? otherLevel - level : level - otherLevel) > range)
                continue;
            if (Group const* group = other->GetGroup())
            {
                if (group->IsFull() || !g_listed.contains(other->GetGUID().GetCounter()))
                    continue;
            }
            ++count;
        }
        return count;
    }
    // the size of the leader's last posted listing, for `.bot lfg` after the timer has run out
    std::unordered_map<uint64, uint32> g_lastSize;
    uint32 g_lastTickMs = 0;

    // The party size a listing is filled to: the activity's entry in Playerbot.Lfg.ActivityMaps (Upper Blackrock Spire is 10, a raid is
    // as big as its map, any other dungeon is a 5-man); the largest of the listed activities. An unknown activity is a 5-man party.
    uint32 PartySizeOf(std::vector<uint32> const& activities)
    {
        uint32 size = kLfgMinPartySize;
        for (uint32 activity : activities)
        {
            auto const& maps = Playerbot::Services::Config().lfg_activity_maps();
            auto const itr = std::ranges::find(maps, activity, &ConfigReader::LfgActivityMap::activity);
            if (itr == maps.end())
                continue;
            MapEntry const* map = sMapStore.LookupEntry(itr->map);
            size = std::max(size, LfgActivityPartySize(itr->size, map && map->IsRaid(), map ? uint32(map->MaxPlayers) : 0));
        }
        return size;
    }

    // What a member counts as when the group carries no role for them: warriors tank, priests heal, the rest deal damage.
    uint8 DefaultRoleForClass(uint8 cls)
    {
        if (cls == CLASS_WARRIOR) return kLfgRoleTank;
        if (cls == CLASS_PRIEST)  return kLfgRoleHealer;
        return kLfgRoleDamage;
    }

    uint8 RoleMaskOf(Player const* leader, ObjectGuid guid, uint8 cls, uint8 leaderRole)
    {
        // a role the member's class cannot play is not counted: a rogue leader who picked Tank still needs a tank
        if (Group const* group = leader->GetGroup())
        {
            if (uint8 const r = LfgRoleForClass(cls, group->GetLfgRoles(guid)))
                return r;
        }
        if (guid == leader->GetGUID())
            if (uint8 const r = LfgRoleForClass(cls, leaderRole))
                return r;
        return DefaultRoleForClass(cls);
    }

    uint8 BotRoleBit(Playerbot::Role role)
    {
        switch (role)
        {
            case Playerbot::Role::Tank:   return kLfgRoleTank;
            case Playerbot::Role::Healer: return kLfgRoleHealer;
            case Playerbot::Role::Dps:    return kLfgRoleDamage;
            default:                      return 0;
        }
    }

    LfgRoleGap GapOf(Player const* leader, Playerbot::Role ownRole, uint32 size)
    {
        std::vector<uint8> roles;
        uint8 const leaderRole = BotRoleBit(ownRole);
        if (Group const* group = leader->GetGroup())
        {
            for (GroupReference const& ref : group->GetMembers())
                if (Player const* member = ref.GetSource())
                    roles.push_back(RoleMaskOf(leader, member->GetGUID(), member->GetClass(), leaderRole));
        }
        else
            roles.push_back(RoleMaskOf(leader, leader->GetGUID(), leader->GetClass(), leaderRole));
        return ComputeLfgRoleGap(roles, size);
    }

    bool InCategory(uint32 categoryId)
    {
        auto const& allowed = Playerbot::Services::Config().lfg_categories();
        return allowed.empty() || std::ranges::find(allowed, categoryId) != allowed.end();
    }

    void Tell(Player* player, std::string const& text)
    {
        if (player && player->GetSession())
            Playerbot::BotLanguage::LocalizedHandler(player->GetSession()).SendSysMessage(text.c_str());
    }

    uint8 PickDpsClass()
    {
        static constexpr uint8 classes[] = { CLASS_MAGE, CLASS_ROGUE, CLASS_HUNTER, CLASS_WARLOCK };
        return classes[urand(0, std::size(classes) - 1)];
    }

    // The group is at its party size and still lacks a tank or healer: a bot dealing damage (never a real player, never the leader)
    // leaves to make room, as a dismissed companion does. False when the group has no such bot.
    bool DropDamageBot(Player* leader, std::string& report)
    {
        Group* group = leader->GetGroup();
        if (!group)
            return false;
        // a bot the owner has not saved on their roster goes first
        Player* leave = nullptr;
        bool leaveSaved = true;
        for (GroupReference const& ref : group->GetMembers())
        {
            Player* member = ref.GetSource();
            if (!member || member == leader || !member->GetSession() || !member->GetSession()->IsBot())
                continue;
            if (RoleMaskOf(leader, member->GetGUID(), member->GetClass(), 0) & (kLfgRoleTank | kLfgRoleHealer))
                continue;
            bool const saved = Playerbot::Services::Owners().GetOwner(member->GetGUID().GetCounter()).saved;
            if (!leave || (leaveSaved && !saved))
            {
                leave = member;
                leaveSaved = saved;
            }
        }
        if (!leave)
            return false;
        ObjectGuid const guid = leave->GetGUID();
        std::string const name = leave->GetName();
        bool const companion = Playerbot::Services::Owners().GetOwner(guid.GetCounter()).account_id == leader->GetSession()->GetAccountId();
        if (!group->RemoveMember(guid, GROUP_REMOVEMETHOD_LEAVE))
            return false;
        // the leader's companion is dismissed (logged out, as `.bot dismiss`); a world bot goes back to its own life
        if (companion)
            Playerbot::Services::SessionMgr().LogoutBot(guid);
        TC_LOG_INFO("playerbot.v2", "[lfg] {}: {} (damage) left the full group to make room for a tank or healer", leader->GetName(), name);
        report += " " + name + " left to make room.";
        return true;
    }

    // Adds the missing roles as the owner's companions. Returns how many were started. A tank or healer missing from a group that is
    // already at its party size takes the place of a damage dealer bot.
    uint32 FillGap(Player* leader, LfgRoleGap gap, uint32 size, std::string& report)
    {
        uint32 started = 0;
        auto add = [&](uint8 cls, Playerbot::Role role, uint8 count)
        {
            for (uint8 i = 0; i < count; ++i)
            {
                if (Group const* group = leader->GetGroup(); group && group->GetMembersCount() >= size)
                {
                    if (role == Playerbot::Role::Dps || !DropDamageBot(leader, report))
                    {
                        report += " The group is full.";
                        return;
                    }
                }
                // A group of more than 5 is a raid: the first companion makes the group, the next ones need it converted before they
                // are invited. The open-world companion cap does not apply here, the dungeon's own limit does (inside an instance).
                if (size > MAX_GROUP_SIZE)
                    if (Group* group = leader->GetGroup(); group && !group->isRaidGroup() && group->GetLeaderGUID() == leader->GetGUID())
                        group->ConvertToRaid();
                std::string full;
                if (leader->GetMap() && leader->GetMap()->IsDungeon() && Fleet::InstanceGroupFull(leader, full))
                {
                    report += " Stopped: " + full + ".";
                    return;
                }
                std::string msg;
                if (!AddCompanionForOwner(leader, role == Playerbot::Role::Dps ? PickDpsClass() : cls, role, msg))
                {
                    report += " " + msg;
                    return;
                }
                ++started;
            }
        };
        add(CLASS_WARRIOR, Playerbot::Role::Tank, gap.tank);
        add(CLASS_PRIEST, Playerbot::Role::Healer, gap.healer);
        add(0, Playerbot::Role::Dps, gap.dps);
        return started;
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
}

void LfgListingChanged(Player* leader, uint32 categoryId, std::vector<uint32> const& activities, bool listed)
{
    if (!leader || !leader->GetSession() || leader->GetSession()->IsBot())
        return;
    uint64 const key = leader->GetGUID().GetCounter();
    if (listed)
    {
        g_listed.insert(key);
        // A player's own dungeon listing shows which category the client lists dungeons in: the bot groups go there too.
        for (ListedActivity const& act : Playerbot::Services::Config().lfg_bot_groups_activities())
            if (std::ranges::find(activities, act.activity) != activities.end())
                g_botGroupCategory = categoryId;
    }
    else
        g_listed.erase(key);
    if (!Playerbot::Services::Config().lfg_enable())
        return;
    if (!listed)
    {
        if (g_pending.erase(key))
            TC_LOG_INFO("playerbot.v2", "[lfg] {} removed the listing: bot fill cancelled", leader->GetName());
        return;
    }

    std::string acts;
    for (uint32 a : activities)
        acts += (acts.empty() ? "" : ",") + std::to_string(a);
    uint32 const seconds = Playerbot::Services::Config().lfg_bot_fill_seconds();
    uint32 const partySize = PartySizeOf(activities);
    g_lastSize[key] = partySize;
    TC_LOG_INFO("playerbot.v2", "[lfg] listing posted by {}: category={} activities={} party_size={} wait={}s", leader->GetName(), categoryId, acts, partySize, seconds);
    if (!seconds || !InCategory(categoryId))
    {
        g_pending.erase(key);
        return;
    }
    // A re-post or edit of the same listing keeps its running timer.
    if (g_pending.contains(key))
        return;
    uint32 const now = getMSTime();
    g_pending[key] = PendingListing{ now + seconds * IN_MILLISECONDS, categoryId, partySize, now };   // checked on the next tick
    if (Playerbot::Services::Config().lfg_instant_fill_if_nobody())
        Tell(leader, "Group Finder: if nobody in your level range is online or listed, your bots fill the missing roles at once; otherwise after " +
             std::to_string(seconds / 60) + " min " + std::to_string(seconds % 60) + " s.");
    else
        Tell(leader, std::string("Group Finder: if nobody joins within ") + std::to_string(seconds / 60) + " min " + std::to_string(seconds % 60) +
             (Playerbot::Services::Config().lfg_trickle() ? " s, your bots come one at a time to fill the missing roles." : " s, your bots fill the missing roles."));
}

void LfgTick(uint32 nowMs)
{
    if (g_pending.empty() || getMSTimeDiff(g_lastTickMs, nowMs) < 1000)
        return;
    g_lastTickMs = nowMs;
    for (auto it = g_pending.begin(); it != g_pending.end();)
    {
        Player* leader = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(it->first));
        if (!leader || !leader->IsInWorld())
        {
            it = g_pending.erase(it);               // logged out
            continue;
        }
        Group const* group = leader->GetGroup();
        if (group && group->GetLeaderGUID() != leader->GetGUID())
        {
            it = g_pending.erase(it);               // joined someone else's group
            continue;
        }
        uint32 const size = it->second.partySize;
        LfgRoleGap const gap = GapOf(leader, Playerbot::Role::Unknown, size);
        if (!gap.Total())
        {
            TC_LOG_INFO("playerbot.v2", "[lfg] {}: group is complete, no bots needed", leader->GetName());
            it = g_pending.erase(it);
            continue;
        }
        // Count the real players who could still join: at once after the post, then every 15 s.
        bool const checkNow = int32(nowMs - it->second.nextCheckMs) >= 0;
        uint32 real = 0;
        if (checkNow)
        {
            real = RealPlayersInRange(leader);
            it->second.nextCheckMs = nowMs + kCheckIntervalMs;
        }
        int32 const msLeft = int32(it->second.deadlineMs - nowMs);
        bool const instantIfNobody = Playerbot::Services::Config().lfg_instant_fill_if_nobody();
        // Between two counts only the wait running out can end the listing; the count of the last check stands.
        LfgFillAction const action = checkNow ? DecideLfgFill(msLeft, real, instantIfNobody)
                                              : (msLeft <= 0 ? LfgFillAction::FillAfterWait : LfgFillAction::Wait);
        if (checkNow)
        {
            if (action == LfgFillAction::FillNow)
                TC_LOG_INFO("playerbot.v2", "[lfg] instant fill: 0 real players in range ({}, level {} +/- {})", leader->GetName(), leader->GetLevel(), Playerbot::Services::Config().lfg_level_range());
            else if (action == LfgFillAction::Wait)
                TC_LOG_INFO("playerbot.v2", "[lfg] waiting: {} real in range ({}, {} s left)", real, leader->GetName(), std::max<int32>(msLeft, 0) / int32(IN_MILLISECONDS));
        }
        if (action == LfgFillAction::Wait)
        {
            // Trickle: one bot at a time, spread so that the last one comes when the wait is over. A place a real player has taken
            // meanwhile is no longer missing; a bot started a moment ago may not be in the group yet, so the bots started so far
            // are taken off the places missing at the start.
            if (Playerbot::Services::Config().lfg_trickle())
            {
                PendingListing& p = it->second;
                if (!p.stepsReady)
                {
                    p.stepsReady = true;
                    p.initialGap = gap;
                    uint32 const total = std::max<uint32>(gap.Total(), 1u);
                    p.stepIntervalMs = std::max<uint32>(uint32(std::max<int32>(msLeft, 0)) / total, 1000u);
                    p.nextStepMs = nowMs + p.stepIntervalMs;
                }
                else if (int32(nowMs - p.nextStepMs) >= 0)
                {
                    LfgRoleGap const due = DueGap(p, gap);
                    LfgRoleGap one;
                    if (due.tank > 0)        one.tank = 1;
                    else if (due.healer > 0) one.healer = 1;
                    else if (due.dps > 0)    one.dps = 1;
                    p.nextStepMs = nowMs + p.stepIntervalMs;
                    if (one.Total())
                    {
                        std::string report;
                        uint32 const started = FillGap(leader, one, size, report);
                        if (started)
                        {
                            p.startedGap.tank += one.tank; p.startedGap.healer += one.healer; p.startedGap.dps += one.dps;
                            TC_LOG_INFO("playerbot.v2", "[lfg] {}: trickle, 1 bot started ({} still due).{}", leader->GetName(), due.Total() - 1, report);
                        }
                        else
                            TC_LOG_INFO("playerbot.v2", "[lfg] {}: trickle, no bot could be started.{}", leader->GetName(), report);
                    }
                }
            }
            ++it;
            continue;
        }
        std::string report;
        LfgRoleGap const rest = DueGap(it->second, gap);
        uint32 const started = FillGap(leader, rest, size, report);
        TC_LOG_INFO("playerbot.v2", "[lfg] {}: {}, party of {}, missing {} -> {} bot(s) started.{}", leader->GetName(),
                    action == LfgFillAction::FillNow ? "nobody in range" : "wait over", size, GapText(rest), started, report);
        Tell(leader, std::string(action == LfgFillAction::FillNow ? "Group Finder: nobody in your level range is available. " : "Group Finder: nobody joined in time. ") +
             std::to_string(started) + " bot(s) are joining (" + GapText(rest) + " missing)." + report);
        it = g_pending.erase(it);
    }
}

bool LfgFillNow(Player* leader, Playerbot::Role ownRole, std::string& msg)
{
    if (!leader)
        return false;
    if (Group const* group = leader->GetGroup(); group && group->GetLeaderGUID() != leader->GetGUID())
    { msg = "Only the group leader can fill the group."; return false; }
    // the size of the listing the leader posted last; a group that is already a raid is at least a 10-man
    uint32 size = kLfgMinPartySize;
    if (auto const itr = g_lastSize.find(leader->GetGUID().GetCounter()); itr != g_lastSize.end())
        size = itr->second;
    if (Group const* group = leader->GetGroup(); group && group->isRaidGroup())
        size = std::max<uint32>(size, 10);
    LfgRoleGap gap;
    std::string report;
    uint32 const started = LfgFillToSize(leader, ownRole, size, gap, report);
    if (!gap.Total())
    { msg = "Your group is already full: " + std::to_string(size) + " players."; return true; }
    TC_LOG_INFO("playerbot.v2", "[lfg] {}: .bot lfg, party of {}, missing {} -> {} bot(s) started.{}", leader->GetName(), size, GapText(gap), started, report);
    msg = std::to_string(started) + " bot(s) are joining (" + GapText(gap) + " missing)." + report;
    g_pending.erase(leader->GetGUID().GetCounter());
    return true;
}

uint32 LfgFillToSize(Player* leader, Playerbot::Role ownRole, uint32 size, LfgRoleGap& gap, std::string& report)
{
    gap = GapOf(leader, ownRole, size);
    g_lastSize[leader->GetGUID().GetCounter()] = size;
    return gap.Total() ? FillGap(leader, gap, size, report) : 0;
}

namespace
{
    // A group the bots made and listed: its leader is a bot, the other members are bots too, until a player joins.
    struct ListedBotGroup
    {
        uint64 leader = 0;                  // guid counter
        uint32 startMs = 0, lifetimeMs = 0;
        bool alliance = true;
        uint32 bots = 0;                    // members, for the bot budget
        uint32 activity = 0;                // the Group Finder activity id it is listed for
        uint32 capacity = 5;                // the party size of the dungeon: a player joining a full group takes a bot's place
    };
    std::vector<ListedBotGroup> g_botGroups;
    uint32 g_botGroupsTickMs = 0, g_nextBotGroupMs = 0;
    bool g_nextBotGroupSet = false;
    uint32 g_searchTryAllianceMs = 0, g_searchTryHordeMs = 0;

    bool IsHuman(Player const* p)
    {
        return p && p->GetSession() && !p->GetSession()->IsBot();
    }

    float Roll() { return float(urand(0, 1000)) / 1000.0f; }

    // The leader of the group a player is in, as a listed bot group, or nullptr.
    ListedBotGroup* ListedGroupOf(Group const* group)
    {
        if (!group)
            return nullptr;
        uint64 const leader = group->GetLeaderGUID().GetCounter();
        auto const itr = std::ranges::find(g_botGroups, leader, &ListedBotGroup::leader);
        return itr != g_botGroups.end() ? &*itr : nullptr;
    }

    // The player took the lead: the listing ends and the bots are their companions (the same owner binding `.playerbot adopt` makes).
    void HandOverToPlayer(Group* group, ObjectGuid const& listedLeader)
    {
        Player* human = nullptr;
        for (GroupReference const& ref : group->GetMembers())
            if (Player* member = ref.GetSource(); IsHuman(member))
            {
                human = member;
                break;
            }
        if (!human)
            return;
        uint32 const partySize = ListedGroupOf(group) ? ListedGroupOf(group)->capacity : kLfgMinPartySize;
        if (Player const* leader = ObjectAccessor::FindConnectedPlayer(listedLeader))
            GroupFinderListings::RemoveBotListing(leader);
        group->ChangeLeader(human->GetGUID());
        uint32 bots = 0;
        for (GroupReference const& ref : group->GetMembers())
            if (Player const* member = ref.GetSource(); member && member != human && !IsHuman(member))
            {
                Playerbot::Services::Owners().SetOwner(member->GetGUID().GetCounter(), human->GetSession()->GetAccountId(), 0);
                ++bots;
            }
        TC_LOG_INFO("playerbot.v2", "[lfg] {} joined the listed bot group of {}: {} bot(s) are now their companions", human->GetName(),
                    listedLeader.ToString(), bots);
        Tell(human, "Group Finder: you are the leader of this group now. Its " + std::to_string(bots) + " bot(s) follow you like your companions.");

        // The open places fill up with companions one at a time, the same as after a posted listing (Lfg.Trickle), so the group
        // reaches its party size (UBRS 10).
        uint32 const seconds = LfgJoinedFillSeconds(Playerbot::Services::Config().lfg_bot_fill_seconds());
        if (seconds && group->GetMembersCount() < partySize)
        {
            uint32 const now = getMSTime();
            g_lastSize[human->GetGUID().GetCounter()] = partySize;
            g_pending[human->GetGUID().GetCounter()] = PendingListing{ now + seconds * IN_MILLISECONDS, 0, partySize, now };
            TC_LOG_INFO("playerbot.v2", "[lfg] {}: the joined group fills its open places up to {} within {} s", human->GetName(), partySize, seconds);
        }
    }

    void EndListedGroup(ListedBotGroup const& listed, char const* why)
    {
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(listed.leader);
        Player* leader = ObjectAccessor::FindConnectedPlayer(guid);
        if (leader)
        {
            GroupFinderListings::RemoveBotListing(leader);
            // nobody joined: the bots go back to their own life
            if (Group* group = leader->GetGroup(); group && group->GetLeaderGUID() == guid)
            {
                bool human = false;
                for (GroupReference const& ref : group->GetMembers())
                    human = human || IsHuman(ref.GetSource());
                if (!human)
                    group->Disband();
            }
        }
        TC_LOG_INFO("playerbot.v2", "[lfg] listed bot group of {} ended: {}", guid.ToString(), why);
    }

    uint32 BotsInListedGroups()
    {
        uint32 total = 0;
        for (ListedBotGroup const& g : g_botGroups)
            total += g.bots;
        return total;
    }

    uint32 ListedGroupsOf(bool alliance)
    {
        return uint32(std::ranges::count_if(g_botGroups, [&](ListedBotGroup const& g) { return g.alliance == alliance; }));
    }

    // The groups wanted for one faction now: the setting at the busiest hour, scaled by the local hour.
    uint32 WantedGroups()
    {
        auto const& config = Playerbot::Services::Config();
        tm const now = TimeBreakdown(GameTime::GetGameTime());
        return ListedGroupTarget(config.lfg_bot_groups_max(), ListedGroupDaylight(uint32(now.tm_hour)));
    }

    // How well a class fills a role (0 = cannot). The archetype role_affinity of a bot is damage-heavy for every archetype, so
    // it says nothing about tanks and healers; the class does.
    uint8 TankFit(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR: return 3;
            case CLASS_PALADIN: return 2;
            case CLASS_DRUID:   return 1;
            default:            return 0;
        }
    }

    uint8 HealerFit(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_PRIEST:  return 3;
            case CLASS_PALADIN:
            case CLASS_DRUID:   return 2;
            case CLASS_SHAMAN:  return 1;
            default:            return 0;
        }
    }

    // The group's members get the roles of the shape, tanks and healers going to the members whose class fits best, so the list shows
    // the mix and not ten damage dealers. Returns the tank and healer places really filled (fewer than the shape when the group has
    // no member of a fitting class: that place is open then).
    LfgRoleGap AssignListedRoles(Group* group, LfgRoleGap have)
    {
        std::vector<Player*> members;
        for (GroupReference const& ref : group->GetMembers())
            if (Player* member = ref.GetSource())
                members.push_back(member);
        LfgRoleGap got;
        auto assign = [&](uint8 (*fit)(uint8), uint8 mask, uint8 count, uint8& filled)
        {
            std::ranges::stable_sort(members, [&](Player* a, Player* b) { return fit(a->GetClass()) > fit(b->GetClass()); });
            for (auto it = members.begin(); it != members.end() && count && fit((*it)->GetClass()); )
            {
                group->SetLfgRoles((*it)->GetGUID(), mask);
                it = members.erase(it);
                --count;
                ++filled;
            }
        };
        assign(TankFit, kLfgRoleTank, have.tank, got.tank);
        assign(HealerFit, kLfgRoleHealer, have.healer, got.healer);
        for (Player* member : members)
            group->SetLfgRoles(member->GetGUID(), kLfgRoleDamage);
        return got;
    }

    // Lists one bot group for players of `alliance` (a faction with a player online): a dungeon of the table, most places taken and
    // one or two of a scarce role open. False when too few free bots could be found.
    bool ListNewBotGroup(uint32 nowMs, bool alliance)
    {
        auto const& config = Playerbot::Services::Config();
        auto const& activities = config.lfg_bot_groups_activities();
        if (activities.empty() || BotsInListedGroups() >= config.lfg_bot_groups_max_bots())
            return false;
        // The dungeons are tried in random order; each is filled only with bots of its own level range, so the level of the listed
        // members fits the dungeon. A dungeon without enough free bots of that range is skipped (a group too small, or without the
        // tank and healer its mix lists, is dissolved again and the next one is tried).
        // A dungeon with no listed group of the faction yet comes first: every dungeon has a group before one has two.
        std::vector<uint32> listedHere;
        for (ListedBotGroup const& g : g_botGroups)
            if (g.alliance == alliance)
                listedHere.push_back(g.activity);
        std::vector<ListedActivity const*> order;
        for (size_t index : ListedActivityOrder(activities, listedHere, [](uint32 n) { return urand(0, n - 1); }))
            order.push_back(&activities[index]);

        ListedActivity const* chosen = nullptr;
        uint32 size = 0;
        ListedGroupShape shape;
        Player* leader = nullptr;
        for (ListedActivity const* act : order)
        {
            size = act->size;
            shape = PlanListedGroup(size, Roll(), Roll(), Roll());

            GroupRequest req{};
            req.required = uint8(shape.members);
            req.want_tank = shape.have.tank;
            req.want_healer = shape.have.healer;
            req.want_dps = shape.have.dps;
            req.faction_mask = alliance ? 1u : 2u;
            req.level_min = act->levelMin;
            req.level_max = act->levelMax;
            req.content_id = act->map;

            uint64 leaderLow = 0;
            Playerbot::Services::GroupBuilder().BuildGroup(req, &leaderLow);
            Player* formed = leaderLow ? ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(leaderLow)) : nullptr;
            Group* formedGroup = formed ? formed->GetGroup() : nullptr;
            if (!formedGroup)
                continue;
            if (formedGroup->GetMembersCount() < std::max<uint32>(3, shape.members > 2 ? shape.members - 2 : 3))
            {
                formedGroup->Disband();
                continue;
            }
            // the roles are the classes' own: the tank and healer places really filled are listed, the rest is open (a group
            // without any tank or healer class is not listed)
            LfgRoleGap const got = AssignListedRoles(formedGroup, shape.have);
            if (!got.tank && !got.healer)
            {
                formedGroup->Disband();
                continue;
            }
            LfgRoleGap const mix = LfgRoleMix(size);
            shape.have.tank = got.tank;
            shape.have.healer = got.healer;
            shape.missing.tank = uint8(mix.tank - got.tank);
            shape.missing.healer = uint8(mix.healer - got.healer);
            chosen = act;
            leader = formed;
            break;
        }
        Group* group = leader ? leader->GetGroup() : nullptr;
        if (!group)
        {
            TC_LOG_INFO("playerbot.v2", "[lfg] no bot group listed: no dungeon of {} has enough free {} bots of its level range with a tank or healer ({} listed, {} bots in listed groups)",
                        activities.size(), alliance ? "Alliance" : "Horde", ListedGroupsOf(alliance), BotsInListedGroups());
            return false;
        }
        ListedActivity const& act = *chosen;
        uint64 const leaderLow = leader->GetGUID().GetCounter();
        // the byte the client writes for a default listing (2 = Relaxed): the one value known to read right
        static constexpr uint8 kDefaultPlayStyle = 2;
        uint32 const category = g_botGroupCategory ? g_botGroupCategory : config.lfg_bot_groups_category();
        GroupFinderListings::PostBotListing(leader, category, { act.activity }, "", "", kDefaultPlayStyle);
        g_botGroups.push_back(ListedBotGroup{ leaderLow, nowMs,
            ListedGroupLifetimeMs(config.lfg_bot_groups_min_seconds(), config.lfg_bot_groups_max_seconds(), Roll()), alliance,
            group->GetMembersCount(), act.activity, size });
        TC_LOG_INFO("playerbot.v2", "[lfg] bot group listed: leader {} activity {} category {} members {}/{} looking for {} ({}; now {} listed)",
                    leader->GetName(), act.activity, category, group->GetMembersCount(), size, GapText(shape.missing),
                    alliance ? "Alliance" : "Horde", g_botGroups.size());
        return true;
    }
}

void LfgBotGroupsTick(uint32 nowMs)
{
    auto const& config = Playerbot::Services::Config();
    if (getMSTimeDiff(g_botGroupsTickMs, nowMs) < 1000)
        return;
    g_botGroupsTickMs = nowMs;
    if (!config.lfg_enable() || !Playerbot::Services::Initialized())
        return;

    for (auto it = g_botGroups.begin(); it != g_botGroups.end();)
    {
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(it->leader);
        Player* leader = ObjectAccessor::FindConnectedPlayer(guid);
        Group* group = leader ? leader->GetGroup() : nullptr;
        bool human = false;
        if (group)
            for (GroupReference const& ref : group->GetMembers())
                human = human || IsHuman(ref.GetSource());
        ListedGroupAction const action = DecideListedGroup(group != nullptr, group ? group->GetMembersCount() : 0, human,
                                                           getMSTimeDiff(it->startMs, nowMs), it->lifetimeMs);
        if (action == ListedGroupAction::Keep)
        {
            ++it;
            continue;
        }
        if (action == ListedGroupAction::Handover)
            HandOverToPlayer(group, guid);
        else
            EndListedGroup(*it, action == ListedGroupAction::Expire ? "expired" : "group gone");
        it = g_botGroups.erase(it);
        g_nextBotGroupSet = false;      // the list is checked again at once: a faction left without a group gets a new one
    }

    // The list holds the wanted number of groups for each faction with a player online (the setting scaled by the local hour). One
    // group is added per pass, every Lfg.BotGroups.IntervalSeconds, a faction with none at once; a failed try is repeated after 15 s.
    if (!g_nextBotGroupSet || int32(nowMs - g_nextBotGroupMs) >= 0)
    {
        bool humanSide[2] = {};      // [0] Alliance, [1] Horde
        for (auto const& [guid, other] : ObjectAccessor::GetPlayers())
            if (other && other->IsInWorld() && IsHuman(other))
                humanSide[other->GetTeam() == ALLIANCE ? 0 : 1] = true;

        uint32 const wanted = WantedGroups();
        bool failed = false;
        for (uint8 side = 0; side < 2; ++side)
        {
            bool const alliance = side == 0;
            if (!humanSide[side] || !config.lfg_bot_groups_max())
                continue;
            // a group too many (the hour got quieter) is not taken down: its listing runs out; up to three are added per pass,
            // and one faction failing does not hold the other back
            for (uint32 added = 0; added < 3 && ListedGroupsOf(alliance) < wanted; ++added)
                if (!ListNewBotGroup(nowMs, alliance))
                {
                    failed = true;
                    break;
                }
        }
        g_nextBotGroupSet = true;
        g_nextBotGroupMs = nowMs + (failed ? 15u : config.lfg_bot_groups_interval_seconds()) * IN_MILLISECONDS;
    }
}

void LfgBotGroupsOnSearch(Player* searcher)
{
    auto const& config = Playerbot::Services::Config();
    if (!IsHuman(searcher) || !config.lfg_enable() || !config.lfg_bot_groups_max() || !Playerbot::Services::Initialized())
        return;
    bool const alliance = searcher->GetTeam() == ALLIANCE;
    // a list with fewer groups of the faction than wanted now is topped up before it is read, so the search shows the full number;
    // a failed try is not repeated within 5 s
    uint32 const aim = WantedGroups();
    if (ListedGroupsOf(alliance) >= aim)
        return;
    uint32 const now = getMSTime();
    uint32& lastTry = alliance ? g_searchTryAllianceMs : g_searchTryHordeMs;
    if (lastTry && getMSTimeDiff(lastTry, now) < 5000)
        return;
    lastTry = now;
    for (uint32 tries = 0; tries < 8 && ListedGroupsOf(alliance) < aim; ++tries)
        if (!ListNewBotGroup(now, alliance))
            break;
}

namespace
{
    // The group is at its dungeon's party size: a bot (never the leader, a damage dealer first) leaves to make room.
    bool MakeRoom(Group* group, ListedBotGroup const& listed)
    {
        if (group->GetMembersCount() < listed.capacity && !group->IsFull())
            return true;
        Player* leave = nullptr;
        bool leaveIsDamage = false;
        for (GroupReference const& ref : group->GetMembers())
        {
            Player* member = ref.GetSource();
            if (!member || IsHuman(member) || member->GetGUID() == group->GetLeaderGUID())
                continue;
            bool const damage = (group->GetLfgRoles(member->GetGUID()) & (kLfgRoleTank | kLfgRoleHealer)) == 0;
            if (!leave || (damage && !leaveIsDamage))
            {
                leave = member;
                leaveIsDamage = damage;
            }
        }
        return leave && group->RemoveMember(leave->GetGUID(), GROUP_REMOVEMETHOD_LEAVE);
    }
}

bool LfgBotGroupCanTake(Player* member, Player* applicant)
{
    if (!IsHuman(applicant) || !Playerbot::Services::Initialized() || applicant->GetGroup())
        return false;
    Group* group = member ? member->GetGroup() : nullptr;
    if (!ListedGroupOf(group))
        return false;
    Player const* leader = ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID());
    return leader && leader->GetTeam() == applicant->GetTeam();
}

Player* LfgBotGroupMemberFor(Player* applicant, uint32 map)
{
    if (!IsHuman(applicant) || !Playerbot::Services::Initialized() || applicant->GetGroup())
        return nullptr;
    bool const alliance = applicant->GetTeam() == ALLIANCE;
    uint32 const level = applicant->GetLevel();
    for (ListedBotGroup const& g : g_botGroups)
    {
        if (g.alliance != alliance)
            continue;
        ListedActivity const* act = nullptr;
        for (ListedActivity const& a : Playerbot::Services::Config().lfg_bot_groups_activities())
            if (a.activity == g.activity) { act = &a; break; }
        if (!act || (map && act->map != map))
            continue;
        if (level + 2 < act->levelMin || level > uint32(act->levelMax) + 2)
            continue;
        Player* leader = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(g.leader));
        if (!leader || !leader->GetGroup() || leader->GetTeam() != applicant->GetTeam())
            continue;
        return leader;
    }
    return nullptr;
}

bool LfgBotGroupJoinRequest(Player* member, Player* applicant)
{
    if (!LfgBotGroupCanTake(member, applicant))
        return false;
    Group* group = member->GetGroup();
    ListedBotGroup* listed = ListedGroupOf(group);
    ObjectGuid const guid = group->GetLeaderGUID();
    if (!MakeRoom(group, *listed) || !group->AddMember(applicant))
        return false;
    HandOverToPlayer(group, guid);
    std::erase_if(g_botGroups, [&](ListedBotGroup const& g) { return g.leader == guid.GetCounter(); });
    return true;
}

} // namespace Playerbot::V2
