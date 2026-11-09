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
    // tank's pull-and-return: ranged opener, run to the rally point, tank the mob there (OrderPullBack). "mount" and
    // "dismount" are the parser's own words (MountIntent / DismountIntent): with AI chat on they went to the language model as
    // chatter and the bots only talked back (Karrz, 2026-11-08). Exact phrases only.
    // The owner's plain-language words (Playerbot.Behavior.OwnerCommands, BotCommandParser OwnerWordOrder) are exact
    // phrases too: summon, passive / defensive / aggressive, the questions who / help / stats / gear / inventory, the
    // quest orders, loot / loot all, sell grey, repair. "drop quest <name>" is an argument order below.
    static constexpr std::array<std::string_view, 20> kGroupOrders =
        { "follow", "stay", "hold", "resume", "attack", "come", "disengage", "assist", "pull", "flee", "release",
          "fallback", "fall back", "pull back", "pullback", "pull and back", "back", "rally here", "mount", "dismount" };
    // "stance nofight" / "stance defensive" / "stance aggressive" (and the older add-on's "stance passive") are the ForeverBots panel's
    // own re-send of its stance (BotCommandParser): stance only.
    static constexpr std::array<std::string_view, 24> kOwnerWords =
        { "summon", "passive", "defensive", "aggressive", "stance passive", "stance aggressive", "stance defensive", "stance nofight", "who", "help", "stats", "gear", "inventory",
          "quests", "quest list", "accept", "accept all", "turn in", "turnin", "share", "loot", "loot all",
          "sell grey", "repair" };
    bool const owner_word = owner_words &&
        (std::find(kOwnerWords.begin(), kOwnerWords.end(), order) != kOwnerWords.end() || order.rfind("drop quest ", 0) == 0);
    // Formation / spacing / main tank orders carry an argument ("formation circle", "spacing tight", "set main tank X").
    // Gear orders: "equip <item link / name / upgrades / best>", "use your best gear" (BotCommandParser OrderEquip).
    // "use <object>" (an altar, brazier, lever, orb; BotCommandParser "use") is unambiguous as a bare party line, so it needs no ";"
    // (Karrz typed "use altar" / "use orb" in party chat and nothing answered, 2026-10-23). A bare "use" alone still says nothing.
    bool const arg_order = order.rfind("gear ", 0) == 0 || order == "equip" || order.rfind("equip ", 0) == 0 || order == "use your best gear" ||
        order.rfind("use ", 0) == 0 ||
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
        order.rfind("crowdcontrol ", 0) == 0 || order.rfind("crowd control ", 0) == 0 || order == "cc on" || order == "cc off" ||
        // The panel's Ignore target button and key: the owner's own list, answered once (BotCommandParser::ApplyIgnoreOrder).
        order == "ignore target" || order == "ignore clear" ||
        // The Positions tab's words: "place <shape> [gap] [here|target] [who...]", answered once (BotCommandParser::ApplyPlaceOrder).
        order.rfind("place ", 0) == 0;
    // Everyday words for the two commonest orders, exact lines only ("follow me please" stays chatter). Karrz typed "wait here" in
    // party chat (2026-11-08): a bot answered "wait, got it" (BotChatReactor's acknowledgement) and nobody waited; "follow me"
    // likewise only got a reply. The Russian lines are the ones the Russian testers use.
    struct Alias { std::string_view words; std::string_view order; };
    // "come here", "mount up" and "get off your mount" likewise reached only the AI chat (Karrz, 2026-11-08).
    static constexpr std::array<Alias, 16> kAliases = { {
        { "wait", "stay" }, { "wait here", "stay" }, { "stay here", "stay" }, { "wait for me", "stay" }, { "hold position", "stay" },
        { "follow me", "follow" }, { "come with me", "follow" }, { "with me", "follow" }, { "come here", "come" },
        { "mount up", "mount" }, { "get mounted", "mount" }, { "get off your mount", "dismount" },
        { "\xD0\xBF\xD0\xBE \xD0\xBA\xD0\xBE\xD0\xBD\xD1\x8F\xD0\xBC", "mount" },                      // "по коням"
        { "\xD0\xB6\xD0\xB4\xD0\xB8", "stay" },                                                        // "жди"
        { "\xD1\x81\xD1\x82\xD0\xBE\xD0\xB9", "stay" },                                                // "стой"
        { "\xD0\xB7\xD0\xB0 \xD0\xBC\xD0\xBD\xD0\xBE\xD0\xB9", "follow" },                              // "за мной"
    } };
    if (!arg_order && !owner_word)
    {
        std::string_view said = order;   // "wait here!" / "follow me." count too
        while (!said.empty() && (said.back() == '!' || said.back() == '.' || said.back() == ' '))
            said.remove_suffix(1);
        for (Alias const& a : kAliases)
            if (said == a.words)
                return std::string(a.order);
    }
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

std::optional<std::string> AddressedGroupOrder(std::string_view msg, std::function<bool(std::string_view)> const& is_group_bot)
{
    if (msg.empty() || msg.front() == ';')
        return std::nullopt;
    auto const begin = msg.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return std::nullopt;
    msg.remove_prefix(begin);
    auto const sep = msg.find_first_of(" \t");
    if (sep == std::string_view::npos)
        return std::nullopt;
    std::string_view name = msg.substr(0, sep);
    while (!name.empty() && (name.back() == ',' || name.back() == ':'))
        name.remove_suffix(1);
    std::string_view rest = msg.substr(sep + 1);
    auto const rest_begin = rest.find_first_not_of(" \t");
    if (name.empty() || rest_begin == std::string_view::npos)
        return std::nullopt;
    rest.remove_prefix(rest_begin);
    rest = rest.substr(0, rest.find_last_not_of(" \t") + 1);
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (lower == "all" || lower == "everyone" || lower == "everybody")
        return "squad:" + std::string(rest);
    if (!is_group_bot(name))
        return std::nullopt;
    return std::string(name) + ":" + std::string(rest);
}

std::optional<std::string> ClassifyAddonOrder(std::string_view prefix, std::string_view text, bool owner_words)
{
    if (prefix != kAddonOrderPrefix)
        return std::nullopt;
    return ClassifyGroupOrder(text, owner_words);
}

}  // namespace Playerbot::V2
