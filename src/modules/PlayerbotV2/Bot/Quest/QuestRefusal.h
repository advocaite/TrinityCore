// QuestRefusal - why a bot could not take a quest, said to the player who asked for it (pure: no game objects, unit tested).
//
// The server's accept path (API::accept_quest / accept_shared_quest, Api/PlayerbotAPI.cpp) runs the same gates as a player's
// accept and records the first one that failed. When the owner ordered the accept ("accept", the AI bots bridge, a shared
// quest), the bot then says which gate it was instead of going quiet. The lines are English templates with `{}` for the quest;
// Util/BotLocaleRu.cpp has every one of them, so a Russian client reads them in Russian.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Playerbot {

enum class QuestRefusal : uint8_t
{
    None = 0,       // nothing refused (or nothing recorded)
    Other,          // the quest is unknown, or a giver the bot cannot take quests from
    Disabled,       // turned off on this server (disables table)
    OnIt,           // already in the quest log
    Done,           // already handed in and not repeatable
    OtherChoice,    // one quest of its exclusive group is already taken or done
    Class,
    Race,
    LowLevel,
    HighLevel,
    Skill,
    Reputation,
    Prerequisite,   // the quest before it (or a breadcrumb's follow-up) is not done
    Timed,          // another timed quest is running
    NotAgainYet,    // a daily / weekly / monthly / seasonal quest already done this time
    Conditions,     // the conditions table says no (a phase, an aura, a world state)
    Expansion,
    LogFull,
    BagsFull,       // the item the quest hands out has no room
    NoGiver,        // the giver or the sharer is gone, or the share ran out
    OutOfRange,
    Unfinishable,   // no objectives and nobody to hand it in to
};

// The template for `why` ("I can't take {}: ..."), or an empty view for None.
inline std::string_view QuestRefusalTemplate(QuestRefusal why)
{
    switch (why)
    {
        case QuestRefusal::None:         return {};
        case QuestRefusal::Other:        return "I can't take {}.";
        case QuestRefusal::Disabled:     return "I can't take {}: it is turned off on this server.";
        case QuestRefusal::OnIt:         return "I already have {}.";
        case QuestRefusal::Done:         return "I already did {}.";
        case QuestRefusal::OtherChoice:  return "I can't take {}: I already took another quest that rules it out.";
        case QuestRefusal::Class:        return "I can't take {}: it is not for my class.";
        case QuestRefusal::Race:         return "I can't take {}: it is not for my race.";
        case QuestRefusal::LowLevel:     return "I can't take {}: my level is too low.";
        case QuestRefusal::HighLevel:    return "I can't take {}: my level is too high.";
        case QuestRefusal::Skill:        return "I can't take {}: I don't have the skill for it.";
        case QuestRefusal::Reputation:   return "I can't take {}: my reputation isn't right for it.";
        case QuestRefusal::Prerequisite: return "I can't take {}: I haven't done the quest that comes before it.";
        case QuestRefusal::Timed:        return "I can't take {}: I'm already on a timed quest.";
        case QuestRefusal::NotAgainYet:  return "I can't take {} again yet.";
        case QuestRefusal::Conditions:   return "I can't take {} yet: something else has to happen first.";
        case QuestRefusal::Expansion:    return "I can't take {}: it is from a later expansion.";
        case QuestRefusal::LogFull:      return "I can't take {}: my quest log is full.";
        case QuestRefusal::BagsFull:     return "I can't take {}: no room in my bags for its item.";
        case QuestRefusal::NoGiver:      return "I can't take {}: whoever offered it is gone.";
        case QuestRefusal::OutOfRange:   return "I can't take {}: I'm too far from the quest giver.";
        case QuestRefusal::Unfinishable: return "I won't take {}: it can't be finished.";
    }
    return {};
}

// The line for `why` with the quest's name in it, or an empty string for None.
inline std::string QuestRefusalLine(QuestRefusal why, std::string_view quest)
{
    std::string_view const t = QuestRefusalTemplate(why);
    size_t const at = t.find("{}");
    if (t.empty() || at == std::string_view::npos)
        return std::string(t);
    std::string out;
    out.reserve(t.size() + quest.size());
    out.append(t.substr(0, at));
    out.append(quest.empty() ? std::string_view("that quest") : quest);
    out.append(t.substr(at + 2));
    return out;
}

} // namespace Playerbot
