// QuestOrders - see QuestOrders.h.

#include "Bot/Quest/QuestOrders.h"
#include "Bot/Quest/CompanionQuest.h"
#include "Bot/Quest/QuestRefusal.h"
#include "Api/PlayerbotAPI.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotRegistry.h"
#include "Threading/IntentQueue.h"
#include "Threading/SnapshotPublisher.h"
#include "Services.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "GameObjectData.h"
#include "CreatureData.h"
#include "QuestDef.h"
#include "GameTime.h"
#include "Log.h"
#include "fmt/format.h"

#include <algorithm>
#include <cctype>

namespace Playerbot {
namespace QuestOrders {

namespace {

constexpr uint32 kOrderWindowMs = 45000;
constexpr size_t kMaxAcceptPerOrder = 8;

std::string Lower(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return r;
}

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

bool StartsWithWord(std::string_view text, std::string_view word, std::string_view& rest)
{
    if (text.size() < word.size() || text.substr(0, word.size()) != word) return false;
    if (text.size() > word.size() && !std::isspace(static_cast<unsigned char>(text[word.size()]))) return false;
    rest = Trim(text.substr(word.size()));
    return true;
}

// Quest id from "123", "quest 123", "|Hquest:123:45|h[Title]|h|r"; 0 when `arg` names none.
uint32 ExtractQuestId(std::string_view arg)
{
    std::string const low = Lower(arg);
    size_t pos = low.find("hquest:");
    if (pos != std::string::npos)
        pos += 7;
    else
    {
        pos = 0;
        if (low.rfind("quest", 0) == 0) pos = 5;
        while (pos < low.size() && std::isspace(static_cast<unsigned char>(low[pos]))) ++pos;
    }
    uint64 v = 0;
    size_t n = 0;
    while (pos < low.size() && std::isdigit(static_cast<unsigned char>(low[pos])) && n < 9)
    {
        v = v * 10 + uint64(low[pos] - '0');
        ++pos; ++n;
    }
    // A bare number must be the whole argument ("12 boars" is a title, not quest 12).
    if (n > 0 && low.find("hquest:") == std::string::npos && pos < low.size())
        return 0;
    return n ? uint32(v) : 0;
}

// Title text of a link "|Hquest:1:2|h[Title]|h|r", else the argument as typed.
std::string TitleNeedle(std::string_view arg)
{
    std::string const low = Lower(arg);
    size_t const a = low.find('['), b = low.find(']');
    if (a != std::string::npos && b != std::string::npos && b > a + 1)
        return low.substr(a + 1, b - a - 1);
    return low;
}

struct Match { uint32 id = 0; std::string title; };

// Quests of the bot's log matching `arg`: "all" / empty = every one; an id / link; else a title substring.
std::vector<Match> MatchLog(BotSnapshot const& snap, std::string_view arg_raw, bool empty_means_all)
{
    std::vector<Match> out;
    std::string_view const arg = Trim(arg_raw);
    bool const all = arg == "all" || arg == "*" || (arg.empty() && empty_means_all);
    uint32 const want_id = all ? 0 : ExtractQuestId(arg);
    std::string const needle = (all || want_id) ? std::string{} : TitleNeedle(arg);
    if (!all && !want_id && needle.empty()) return out;
    for (auto const& q : snap.quest_log.quests)
    {
        std::string title = CompanionQuest::QuestLabel(q.quest_id);
        bool hit = all || (want_id && q.quest_id == want_id);
        if (!hit && !needle.empty())
            hit = Lower(title).find(needle) != std::string::npos;
        if (hit) out.push_back({q.quest_id, std::move(title)});
    }
    return out;
}

// Objective noun for a log line: creature / item / object name.
std::string ObjectiveName(QuestObjectiveEntry const& o)
{
    if (o.type == 0)
        if (CreatureTemplate const* c = sObjectMgr->GetCreatureTemplate(uint32(o.object_id)))
            return c->Name;
    if (o.type == 1)
        if (ItemTemplate const* i = sObjectMgr->GetItemTemplate(uint32(o.object_id)))
            return i->GetDefaultLocaleName();
    if (o.type == 2)
        if (GameObjectTemplate const* g = sObjectMgr->GetGameObjectTemplate(uint32(o.object_id)))
            return g->name;
    return "objective";
}

struct Target
{
    IntentQueue* q = nullptr;
    IntentId*    next = nullptr;
    BotId        id = 0;
};

Target TargetOf(Player* bot)
{
    Target t;
    if (!bot) return t;
    t.id = bot->GetGUID().GetCounter();
    t.q = Services::Registry().intents(t.id);
    t.next = Services::Registry().next_intent_id(t.id);
    return t;
}

std::shared_ptr<BotSnapshot const> SnapOf(Player* bot)
{
    return bot ? Services::Snapshots().latest(bot->GetGUID().GetCounter()) : nullptr;
}

void LogOrder(Player* bot, char const* what, uint32 quest_id)
{
    TC_LOG_INFO("playerbot.v2", "[quest] {} {} {} (owner order)", bot->GetName(), what, CompanionQuest::QuestLabel(quest_id));
}

} // namespace

std::vector<std::string> List(Player* /*sender*/, Player* bot, std::string_view arg)
{
    std::vector<std::string> out;
    auto snap = SnapOf(bot);
    if (!snap || snap->quest_log.quests.empty())
    {
        out.push_back("I have no quests in my log.");
        return out;
    }
    std::string_view const a = Trim(arg);
    bool const only_done = a == "done" || a == "completed" || a == "complete";
    bool const only_open = a == "open" || a == "incomplete" || a == "active";
    uint32 done = 0;
    for (auto const& q : snap->quest_log.quests)
        if (q.state == 1) ++done;
    out.push_back(fmt::format("Quests: {} ({} done, {} open)", snap->quest_log.quests.size(), done,
        snap->quest_log.quests.size() - done));
    for (auto const& q : snap->quest_log.quests)
    {
        if (only_done && q.state != 1) continue;
        if (only_open && q.state == 1) continue;
        std::string line = fmt::format("{} (L{}, id {})", CompanionQuest::QuestLabel(q.quest_id), q.level, q.quest_id);
        if (q.state == 1)
            line += ": done, ready to turn in";
        else
        {
            bool first = true;
            for (auto const& o : q.objectives)
            {
                if (o.amount <= 0 || (o.flags & (QUEST_OBJECTIVE_FLAG_HIDDEN | QUEST_OBJECTIVE_FLAG_PART_OF_PROGRESS_BAR))) continue;
                line += first ? ": " : ", ";
                line += fmt::format("{}/{} {}", std::min(o.progress, o.amount), o.amount, ObjectiveName(o));
                first = false;
            }
        }
        out.push_back(std::move(line));
    }
    return out;
}

std::vector<std::string> AcceptAll(Player* sender, Player* bot, std::string_view arg)
{
    std::vector<std::string> out;
    auto snap = SnapOf(bot);
    Target t = TargetOf(bot);
    if (!snap || !t.q || !t.next)
    {
        out.push_back("I can't take quests right now.");
        return out;
    }
    BotIntentEmitter emit(t.q, t.id, 0, t.next);
    uint32 taken = 0;

    // A quest the group just shared with me.
    if (snap->social_events.shared_quest_id != 0)
    {
        emit.accept_shared_quest(sender ? sender->GetGUID() : ObjectGuid::Empty);
        LogOrder(bot, "accept shared", snap->social_events.shared_quest_id);
        out.push_back(fmt::format("Taking the shared quest: {}", CompanionQuest::QuestLabel(snap->social_events.shared_quest_id)));
        ++taken;
    }

    // Offers of the NPCs and quest objects next to me (the snapshot's own can-take filter), one named quest or all.
    std::string_view const a = Trim(arg);
    uint32 const want = (a.empty() || a == "all" || a == "*") ? 0 : ExtractQuestId(a);
    std::string const needle = (a.empty() || a == "all" || a == "*" || want) ? std::string{} : TitleNeedle(a);

    // Quests that start from an item in my bags (a dropped letter, a head, a wanted note): taken from the item at once, as a
    // player does with a right click. The bot's own idle:quest_start_item does this too, a few seconds later.
    for (auto const& si : snap->quest_discovery.quest_starting_items)
    {
        if (taken >= kMaxAcceptPerOrder) break;
        if (si.item_guid.IsEmpty()) continue;
        if (want && si.quest_id != want) continue;
        if (!needle.empty() && Lower(CompanionQuest::QuestLabel(si.quest_id)).find(needle) == std::string::npos) continue;
        emit.accept_quest(si.item_guid, si.quest_id, sender ? sender->GetGUID() : ObjectGuid::Empty);
        LogOrder(bot, "accept from item", si.quest_id);
        out.push_back(fmt::format("Starting from an item in my bags: {}", CompanionQuest::QuestLabel(si.quest_id)));
        ++taken;
    }
    for (auto const& off : snap->quest_discovery.quest_offers)
    {
        if (taken >= kMaxAcceptPerOrder) break;
        if (want && off.quest_id != want) continue;
        if (!needle.empty() && Lower(CompanionQuest::QuestLabel(off.quest_id)).find(needle) == std::string::npos) continue;
        emit.accept_quest(off.giver, off.quest_id, sender ? sender->GetGUID() : ObjectGuid::Empty);
        LogOrder(bot, "accept", off.quest_id);
        out.push_back(fmt::format("Accepting: {}", CompanionQuest::QuestLabel(off.quest_id)));
        ++taken;
    }

    // Walk to the NPCs close to me and take what they offer (the idle:companion_quest_accept rule), for 45 s.
    if (want == 0 && needle.empty())
    {
        CompanionQuest::ArmLatch(t.id, CompanionQuest::Latch::AcceptAll, GameTime::GetGameTimeMS(), kOrderWindowMs);
        if (taken == 0)
            out.push_back("Nothing on offer right here; I'll pick up what the quest givers near us have for the next 45 seconds.");
    }
    else if (taken == 0)
    {
        // A quest named by id or link that the offers left out: the offers only hold what the bot can take, so say which gate
        // keeps it out (level, class, done, log full ...) before saying nobody here has it.
        QuestRefusal const why = want ? API(bot).why_cant_take_quest(want) : QuestRefusal::None;
        if (why != QuestRefusal::None && why != QuestRefusal::Other)
            out.push_back(QuestRefusalLine(why, CompanionQuest::QuestLabel(want)));
        else
            out.push_back("That quest isn't on offer from anyone next to me.");
    }
    return out;
}

std::vector<std::string> Drop(Player* /*sender*/, Player* bot, std::string_view arg)
{
    std::vector<std::string> out;
    auto snap = SnapOf(bot);
    Target t = TargetOf(bot);
    if (!snap || !t.q || !t.next)
    {
        out.push_back("I can't drop quests right now.");
        return out;
    }
    if (Trim(arg).empty())
    {
        out.push_back("Drop which quest? Give its name, link or id, or \"all\".");
        return out;
    }
    std::vector<Match> const hits = MatchLog(*snap, arg, /*empty_means_all*/ false);
    if (hits.empty())
    {
        out.push_back(fmt::format("I have no quest matching \"{}\".", std::string(Trim(arg))));
        return out;
    }
    BotIntentEmitter emit(t.q, t.id, 0, t.next);
    for (Match const& m : hits)
    {
        emit.abandon_quest(m.id);
        LogOrder(bot, "drop", m.id);
    }
    if (hits.size() == 1)
        out.push_back(fmt::format("Dropping: {}", hits.front().title));
    else
        out.push_back(fmt::format("Dropping {} quests.", hits.size()));
    return out;
}

std::vector<std::string> TurnIn(Player* /*sender*/, Player* bot, std::string_view arg)
{
    std::vector<std::string> out;
    auto snap = SnapOf(bot);
    Target t = TargetOf(bot);
    if (!snap || !t.q || !t.next)
    {
        out.push_back("I can't turn in quests right now.");
        return out;
    }
    uint32 done = 0;
    for (auto const& q : snap->quest_log.quests)
        if (q.state == 1) ++done;
    if (done == 0)
    {
        out.push_back("None of my quests is complete.");
        return out;
    }
    std::string_view const a = Trim(arg);
    uint32 const want = (a.empty() || a == "all" || a == "*") ? 0 : ExtractQuestId(a);
    std::string const needle = (a.empty() || a == "all" || a == "*" || want) ? std::string{} : TitleNeedle(a);
    BotIntentEmitter emit(t.q, t.id, 0, t.next);
    uint32 handed = 0;
    // The quest NPCs I stand beside: hand in now. The class scorer picks the reward (0xFF).
    for (auto const& tin : snap->quest_discovery.quest_turnins)
    {
        if (want && tin.quest_id != want) continue;
        if (!needle.empty() && Lower(CompanionQuest::QuestLabel(tin.quest_id)).find(needle) == std::string::npos) continue;
        emit.complete_quest(tin.giver, tin.quest_id, /*reward_choice*/ 0xFF);
        LogOrder(bot, "turn_in", tin.quest_id);
        out.push_back(fmt::format("Handing in: {}", CompanionQuest::QuestLabel(tin.quest_id)));
        ++handed;
    }
    // Not beside the ender: walk to the NPCs near me for 45 s (idle:companion_quest_turnin).
    if (want == 0 && needle.empty())
        CompanionQuest::ArmLatch(t.id, CompanionQuest::Latch::TurnInAll, GameTime::GetGameTimeMS(), kOrderWindowMs);
    if (handed == 0)
        out.push_back(want || !needle.empty()
            ? "That quest isn't complete, or its NPC isn't next to me."
            : fmt::format("{} quest(s) are done; I'll hand them in at the NPC near us.", done));
    return out;
}

std::vector<std::string> Share(Player* sender, Player* bot, std::string_view arg)
{
    std::vector<std::string> out;
    auto snap = SnapOf(bot);
    Target t = TargetOf(bot);
    if (!snap || !t.q || !t.next)
    {
        out.push_back("I can't share quests right now.");
        return out;
    }
    std::vector<Match> const hits = MatchLog(*snap, arg, /*empty_means_all*/ true);
    bool const named = !Trim(arg).empty() && Trim(arg) != "all" && Trim(arg) != "*";
    BotIntentEmitter emit(t.q, t.id, 0, t.next);
    uint32 shared = 0;
    for (Match const& m : hits)
    {
        if (shared >= 5) break;
        Quest const* tmpl = sObjectMgr->GetQuestTemplate(m.id);
        if (!tmpl) continue;
        // Without a name, only what the sender doesn't have and could take.
        if (!named && sender)
        {
            if (sender->GetQuestStatus(m.id) != QUEST_STATUS_NONE) continue;
            if (!sender->CanTakeQuest(tmpl, false)) continue;
        }
        emit.share_quest(m.id);
        LogOrder(bot, "share", m.id);
        out.push_back(fmt::format("Sharing: {}", m.title));
        ++shared;
    }
    if (shared == 0)
        out.push_back(named ? fmt::format("I have no quest matching \"{}\" to share.", std::string(Trim(arg)))
                            : std::string("I have nothing you could take from me."));
    return out;
}

bool ParseVerb(std::string_view text, std::string& verb, std::string& arg)
{
    std::string_view t = Trim(text);
    std::string_view rest;
    if (t == "quests" || t == "ql" || t == "q" || t == "quest list" || t == "quests list")
    { verb = "list"; arg.clear(); return true; }
    if (StartsWithWord(t, "quests", rest) || StartsWithWord(t, "ql", rest))
    { verb = "list"; arg = std::string(rest); return true; }
    if (StartsWithWord(t, "accept", rest))
    {
        // "accept quest 12", "accept all", "accept *", "accept <title>"
        std::string_view r2;
        if (StartsWithWord(rest, "quest", r2)) rest = r2;
        verb = "accept"; arg = std::string(rest); return true;
    }
    if (StartsWithWord(t, "drop", rest))
    {
        std::string_view r2;
        if (StartsWithWord(rest, "quest", r2)) rest = r2;
        verb = "drop"; arg = std::string(rest); return true;
    }
    if (StartsWithWord(t, "turn", rest))
    {
        std::string_view r2;
        if (StartsWithWord(rest, "in", r2)) { verb = "turnin"; arg = std::string(r2); return true; }
    }
    for (std::string_view w : {"turnin", "turn_in", "turn-in", "handin", "hand_in"})
        if (StartsWithWord(t, w, rest)) { verb = "turnin"; arg = std::string(rest); return true; }
    if (StartsWithWord(t, "hand", rest))
    {
        std::string_view r2;
        if (StartsWithWord(rest, "in", r2)) { verb = "turnin"; arg = std::string(r2); return true; }
    }
    if (StartsWithWord(t, "share", rest))
    {
        std::string_view r2;
        if (StartsWithWord(rest, "quest", r2)) rest = r2;
        verb = "share"; arg = std::string(rest); return true;
    }
    return false;
}

std::vector<std::string> Run(std::string const& verb, Player* sender, Player* bot, std::string const& arg)
{
    if (verb == "list")   return List(sender, bot, arg);
    if (verb == "accept") return AcceptAll(sender, bot, arg);
    if (verb == "drop")   return Drop(sender, bot, arg);
    if (verb == "turnin") return TurnIn(sender, bot, arg);
    if (verb == "share")  return Share(sender, bot, arg);
    return {};
}

} // namespace QuestOrders
} // namespace Playerbot
