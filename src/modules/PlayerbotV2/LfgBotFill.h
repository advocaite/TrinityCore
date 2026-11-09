// LfgBotFill.h: bots for a Group Finder listing. A player posts a dungeon listing; if the group is not complete
// Playerbot.Lfg.BotFillSeconds later (default 180) the missing tank / healer / damage places are filled with the
// owner's own level-matched companions. Real players always come first: they are counted before any bot is added.
#pragma once

#include "Define.h"
#include "Bot/BotTypes.h"
#include "LfgRoleGap.h"

#include <string>
#include <vector>

class Player;

namespace Playerbot::V2
{

// Module side (LfgBotFill.cpp). All world thread.
void LfgListingChanged(Player* leader, uint32 categoryId, std::vector<uint32> const& activities, bool listed);
void LfgTick(uint32 nowMs);
// `.bot lfg [tank|healer|dps]`: fill now. `ownRole` is the caller's role when they said it (Unknown = from their group role/class).
bool LfgFillNow(Player* leader, Playerbot::Role ownRole, std::string& msg);
// Bot-made groups in the Group Finder list (Playerbot.Lfg.BotGroups.*): every second from the world tick.
void LfgBotGroupsTick(uint32 nowMs);
// A player searched the list: list a bot group for their faction now when there is none (a failed try is not repeated within 5 s).
void LfgBotGroupsOnSearch(Player* searcher);
// A player invited a member of a listed bot group: they join, take the lead and the bots become their companions. False when the
// group is not a listed bot group (or is full), the usual "already in a group" answer then follows.
bool LfgBotGroupJoinRequest(Player* member, Player* applicant);
// True when `member` is in a listed bot group that `applicant` (a player without a group, same faction) can join now: a player's
// whisper "invite" to one of its bots then makes the same join (a bot leaves when the group is full).
bool LfgBotGroupCanTake(Player* member, Player* applicant);
// A member (the leader) of a listed bot group that `applicant` (a player without a group) could join, for the dungeon on `map`
// (0 = any) and about his level; nullptr when none. A channel LFG line is answered from it (Bot/ChannelChat.cpp).
Player* LfgBotGroupMemberFor(Player* applicant, uint32 map);
// Creates one companion for the owner (shared with `.bot add`). In hired mode it is a finder loan bot (HireMgr::SetLoaned).
bool AddCompanionForOwner(Player* owner, uint8 cls, Playerbot::Role role, std::string& msg);
// Fills the leader's group up to `size` now with level-matched companions (`.bot lfg` and the dungeon finder). `gap` is what was
// missing; returns the bots started, `report` says why any could not be.
uint32 LfgFillToSize(Player* leader, Playerbot::Role ownRole, uint32 size, LfgRoleGap& gap, std::string& report);

// The bot dungeon finder (DungeonFinder.cpp), `.bot dungeon`: pick a dungeon (or any for your level), the missing roles are filled
// with your companions and the group is put inside the dungeon's door; `leave` brings you back to where you queued. World thread.
bool DungeonFinderQueue(Player* leader, std::string const& what, Playerbot::Role ownRole, std::string& msg);
bool DungeonFinderLeave(Player* player, std::string& msg);
bool DungeonFinderCancel(Player* player, std::string& msg);
std::string DungeonFinderList(Player* player);
// Where the player stands with the dungeon finder, for the add-on's Group finder tab: phase is "idle", "forming" (queued, bots
// joining), "entering", "leaving" or "inside" (in a dungeon, not on the way). canLeave = `.bot dungeon leave` knows a way out.
struct DungeonFinderState
{
    std::string phase = "idle";
    std::string name;
    uint32 waitedSec = 0;
    uint32 formSec = 0;
    uint32 members = 0;
    uint32 size = 0;
    bool canLeave = false;
};
DungeonFinderState DungeonFinderStatus(Player* player);
void DungeonFinderTick(uint32 nowMs);

} // namespace Playerbot::V2
