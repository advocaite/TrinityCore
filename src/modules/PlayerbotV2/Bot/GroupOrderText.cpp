#include "GroupOrderText.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Playerbot::V2 {

std::optional<std::string> ClassifyGroupOrder(std::string_view msg, bool owner_words)
{
    if (msg.empty() || msg.front() == ';')
        return std::nullopt;
    std::string order(msg);
    std::transform(order.begin(), order.end(), order.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    order.erase(0, order.find_first_not_of(" \t"));
    order.erase(order.find_last_not_of(" \t") + 1);
    if (order.empty())
        return std::nullopt;
    // "assist" is "attack my target"; "pull" sends the tanks at it and the rest follow once it is engaged; "flee"
    // breaks off and runs back to the sender; "release" makes every dead bot release its spirit and run back to its
    // corpse (living ones ignore it).
    // "fallback" (also "fall back", "back") runs every bot to the rally point and re-forms there; "rally here"
    // sets that point (BotCommandParser OrderFallback). "pull back" (also "pullback", "pull and back") is the main
    // tank's pull-and-return: ranged opener, run to the rally point, tank the mob there (OrderPullBack). Exact
    // phrases only.
    // The owner's plain-language words (Playerbot.Behavior.OwnerCommands, BotCommandParser OwnerWordOrder) are exact
    // phrases too: summon, passive / defensive / aggressive, the questions who / help / stats / gear / inventory, the
    // quest orders, loot / loot all, sell grey, repair. "drop quest <name>" is an argument order below.
    static constexpr std::array<std::string_view, 18> kGroupOrders =
        { "follow", "stay", "hold", "resume", "attack", "come", "disengage", "assist", "pull", "flee", "release",
          "fallback", "fall back", "pull back", "pullback", "pull and back", "back", "rally here" };
    static constexpr std::array<std::string_view, 20> kOwnerWords =
        { "summon", "passive", "defensive", "aggressive", "who", "help", "stats", "gear", "inventory",
          "quests", "quest list", "accept", "accept all", "turn in", "turnin", "share", "loot", "loot all",
          "sell grey", "repair" };
    bool const owner_word = owner_words &&
        (std::find(kOwnerWords.begin(), kOwnerWords.end(), order) != kOwnerWords.end() || order.rfind("drop quest ", 0) == 0);
    // Formation / spacing / main tank orders carry an argument ("formation circle", "spacing tight", "set main tank X").
    // Gear orders: "equip <item link / name / upgrades / best>", "use your best gear" (BotCommandParser OrderEquip).
    bool const arg_order = order.rfind("gear ", 0) == 0 || order == "equip" || order.rfind("equip ", 0) == 0 || order == "use your best gear" ||
        order == "wear your best gear" || order.rfind("formation ", 0) == 0 || order.rfind("spacing ", 0) == 0 ||
        order == "spread out" || order == "spaced out" || order == "tight" || order == "close" ||
        order == "maintank" || order == "main tank" || order == "set main tank" ||
        order.rfind("set main tank ", 0) == 0 || order.rfind("main tank ", 0) == 0 || order.rfind("maintank ", 0) == 0 ||
        // The ForeverBots panel's icon assignment, follow distance and tank-marker orders (each bot stores its own part
        // and only the lowest-guid bot answers, see BotCommandParser OrderAssign / OrderDistance / OrderTankMarker).
        order.rfind("assign ", 0) == 0 || order.rfind("distance ", 0) == 0 || order.rfind("follow distance ", 0) == 0 ||
        order.rfind("tank ", 0) == 0 || order == "clear tanks" || order == "clear tank" ||
        order.rfind("rti ", 0) == 0 || order.rfind("pull rti", 0) == 0 ||
        // The panel's CC switch: "crowdcontrol on|off" (also "cc on|off"), answered once for the whole group.
        order.rfind("crowdcontrol ", 0) == 0 || order.rfind("crowd control ", 0) == 0 || order == "cc on" || order == "cc off";
    if (!arg_order && !owner_word && std::find(kGroupOrders.begin(), kGroupOrders.end(), order) == kGroupOrders.end())
        return std::nullopt;
    if (order == "assist")
        order = "attack";
    // "pull rti" is a Pull on the marked mob: one tank pulls the skull (else cross) when nothing is selected. Read as its own words it
    // reached every bot, and every tank-role bot pulled the owner's selection.
    if (order.rfind("pull rti", 0) == 0)
        order = "pull";
    if (order == "pullback" || order == "pull and back")
        order = "pull back";
    return order;
}

std::optional<std::string> ClassifyAddonOrder(std::string_view prefix, std::string_view text, bool owner_words)
{
    if (prefix != kAddonOrderPrefix)
        return std::nullopt;
    return ClassifyGroupOrder(text, owner_words);
}

}  // namespace Playerbot::V2
