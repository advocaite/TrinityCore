// BotCommandParser - Parses chat-driven bot commands ("follow", "stay",
// "attack", "release") and translates them into intents on the bot's queue.
//
// Runs on the world thread (called from the whisper hook). Keeps it lock-free
// by pushing into the per-bot IntentQueue, which the world drain consumes.

#pragma once

#include "BotTypes.h"
#include <string>
#include <vector>

class Group;
class Player;
struct Position;

namespace Playerbot {

class BotCommandParser
{
public:
    // Returns true if the message matched a known command (regardless of
    // success). A future iteration will return rich error info; the bool
    // shape is enough for "did the bot handle this?".
    //
    // Performs squad-address resolution at the top: if `msg` is prefixed
    // with `all:`, `tank:`, `Areon:`, etc., the command body is dispatched
    // to every bot the sender owns that matches the prefix, and the
    // whispered `bot_player` is used only as the messenger that replies
    // with a single summary line. The address resolver is in
    // BotAddressResolver.h.
    // `via` names the source for the [owner_order] log line (whisper / party / addon).
    static bool Dispatch(Player* sender, Player* bot_player, std::string const& msg, char const* via = "whisper");

    // Single-bot dispatch. Skips the address resolver — used internally
    // by Dispatch when a prefix has already been parsed and a per-target
    // command needs to run, and by tests that bypass the prefix layer.
    static bool DispatchSingle(Player* sender, Player* bot_player, std::string const& msg, char const* via = "whisper");

    // The owner's crowd-control orders ("no fear", "@warlock no fear", "@mages no sheep", "yes sheep", "off cc", "on cc"; CcDeny.h):
    // when `line` is one, stores it for the sender's account (OwnerRegistry::SetCcDeny, the marked-CC switch for "off cc" / "on cc"),
    // answers the sender once and returns true. The bots pick it up with their next snapshot. World thread.
    static bool ApplyCcOrder(Player* sender, std::string const& line, char const* via);
    // The owner's "ignore target" (toggles the selected mob on the sender's ignore list; the panel's Ignore target button and key) and
    // "ignore clear" (empties it): the sender's bots never attack a mob on it (OwnerRegistry::ToggleFightIgnore). Stored once for the sender's
    // account and answered once. Returns false for any other line.
    static bool ApplyIgnoreOrder(Player* sender, std::string const& line, char const* via);

    // The ForeverBots Positions tab (add-on PLACE_REQ) and the "place" order: the chosen bots of the sender's group stand in `shape`
    // (close, circle, arc, spread, line, column, spear, arrow; Placement.h) with `gap` (tight, normal, wide) round `anchor` (here =
    // where the sender stands, target = his target) and stay there; after a fight they walk back to their own spot. Shape "follow":
    // they follow again. `who`: bot names and the words all, tanks, healers, melee, ranged, dps, group1..group8, a class; empty = all.
    // Returns how many were placed and the line to answer. World thread.
    struct PlaceResult { uint32 placed = 0; uint32 asked = 0; std::string line; };
    static PlaceResult PlaceBots(Player* sender, std::string const& shape, std::string const& gap, std::string const& anchor,
                                 std::vector<std::string> const& who, char const* via, Position const* point = nullptr);
    // anchor "cursor" without a point arms the order for 30 s: the player's next ping (Module::OnGroupPingPoint) is the spot. Returns
    // true when this ping was taken for it (the ping does nothing else then). World thread.
    static bool TakePendingPlace(Player* sender, Position const& point);
    // "place <shape> [tight|normal|wide] [here|target] [who...]" / "place follow [who...]": PlaceBots once, answered as a system line.
    // Returns false for any other line.
    static bool ApplyPlaceOrder(Player* sender, std::string const& line, char const* via);

    // May `sender` give this bot a group order (the panel, a party / raid line)? Its owner, or for a bot with no owner a player
    // who is its controller in the group they share (GroupControl.h): the group leader when human, else the first human
    // member. An invited world bot obeys the leader exactly like a companion.
    static bool ControlsForGroupOrder(Player const* sender, Player const* bot_player);

    // The bot's role as its owner set it (`.bot add <class> tank`, setrole), else as its spec reads.
    static Role BotRole(Player* bot_player);

    // Latch a "release" order on a dead bot (the "release" / "release_all"
    // commands, and the owner's own Release Spirit via Module::OnSpiritReleased).
    // `why` is logged. World thread. Returns true when the bot is dead and holds
    // an order for this death; living bots ignore it.
    static bool OrderRelease(Player const* sender, Player const* bot, char const* why = "release order");

    // Drop the owner's "follow" order on `bot` (BotAI::owner_follow_order) and log [owner_order]
    // when it was set: the party-chat "pull" hands the fight back to the non-tanks this way.
    static void ReleaseOwnerFollowOrder(Player const* sender, Player const* bot, char const* why);

    // The group's marker assignments ("assign <name> <marker> [tank|cc|kill]"), highest marker first: tank marks, then each
    // bot's kill and cc marks. Read by the "assign list" order and the add-on's hidden ASSIGN_REQ. World thread.
    struct AssignRow { uint8 marker; std::string name; char const* role; };
    static std::vector<AssignRow> AssignList(Group* grp);

private:
    // The command table; DispatchSingle wraps it with the [owner_order] log line.
    static bool DispatchSingleImpl(Player* sender, Player* bot_player, std::string const& msg);
};

} // namespace Playerbot
