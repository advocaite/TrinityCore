// QuestOrders - the owner's quest orders to a companion, as callable functions for BotCommandParser.
//
// Every function runs on the WORLD thread (the whisper / party-chat hook, where BotCommandParser::Dispatch
// runs): it reads the bot's latest published snapshot and static data (ObjectMgr) and QUEUES intents on the
// bot's own intent queue; it never changes the bot's Player (it reads it only through API::why_cant_take_quest, to say
// why a named quest is not on offer). The sender's Player is only read (quest status, CanTakeQuest) to filter what is
// worth sharing. An accept it queues carries the sender, so a refusal on the world thread is whispered back with its
// reason (Bot/Quest/QuestRefusal.h). It returns the reply lines for the owner; the caller whispers
// them (WhisperIntent{sender->GetName(), line} through Push, or Chat). Nothing is whispered here.
//
// `arg` is the text after the verb, trimmed: a quest id, a quest link ("|Hquest:123:45|h[Title]|h|r"), a title
// or a part of one, "all", or empty. Matching is case-insensitive over the bot's own quest log.
//
// Verbs for BotCommandParser (suggested aliases in brackets):
//   "quests" / "ql"   [also "quest list", "q"]        -> QuestOrders::List(sender, bot, arg)
//   "accept"          [also "accept all", "accept *", "accept <quest>"]  -> QuestOrders::AcceptAll(sender, bot, arg)
//   "drop"            ["drop <quest>", "drop all", "abandon" stays as is] -> QuestOrders::Drop(sender, bot, arg)
//   "turn in"         ["turnin", "turn_in", "complete", "turn in <quest>"] -> QuestOrders::TurnIn(sender, bot, arg)
//   "share"           ["share <quest>", "share all"]                       -> QuestOrders::Share(sender, bot, arg)
//
// "accept" and "turn in" also arm a 45 s window (CompanionQuest latches) in which the companion walks to the
// quest NPCs within 40 y of it (the owner need not hold the quest / stand at the NPC) and takes / hands in
// what it can, through the idle:companion_quest_* rules.

#pragma once

#include <string>
#include <string_view>
#include <vector>

class Player;

namespace Playerbot {
namespace QuestOrders {

std::vector<std::string> List(Player* sender, Player* bot, std::string_view arg = {});
std::vector<std::string> AcceptAll(Player* sender, Player* bot, std::string_view arg = {});
std::vector<std::string> Drop(Player* sender, Player* bot, std::string_view arg);
std::vector<std::string> TurnIn(Player* sender, Player* bot, std::string_view arg = {});
std::vector<std::string> Share(Player* sender, Player* bot, std::string_view arg = {});

// Parse a "quests" family verb line: `text` is the owner's message lower-cased and trimmed. On a match returns
// true and sets `verb` to one of "list" / "accept" / "drop" / "turnin" / "share" and `arg` to the rest.
// Recognised: quests, quest list, ql, q (list); accept, accept all, accept *, accept <q> (accept);
// drop <q>, drop all, drop quest <q> (drop); turn in, turnin, turn_in, hand in (turn in); share, share <q>,
// share quest <q> (share). `abandon` / `abandonall` are not claimed (the parser already has them).
bool ParseVerb(std::string_view text, std::string& verb, std::string& arg);

// Run the parsed verb: List / AcceptAll / Drop / TurnIn / Share by name. Empty vector for an unknown verb.
std::vector<std::string> Run(std::string const& verb, Player* sender, Player* bot, std::string const& arg);

} // namespace QuestOrders
} // namespace Playerbot
