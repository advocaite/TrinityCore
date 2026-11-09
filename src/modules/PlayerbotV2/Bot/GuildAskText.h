// GuildAskText - does a whispered line ask the bot for an invite to its guild? English and Russian. Pure text logic (no game
// objects), unit tested offline (tests/game/ClassicGuildAskText.cpp). GuildWhisperInvite.cpp acts on the answer.

#pragma once

#include <string>
#include <string_view>

namespace Playerbot::V2::GuildAsk {

// Lower case (ASCII and Russian letters, Ё included), punctuation to spaces, one space between words, apostrophes dropped.
std::string Normalize(std::string_view raw);

// The line asks to join the bot's guild: a guild word ("guild", "гильдию", "ги", "клан", ...) with an ask word ("invite", "join",
// "can i", "pls", "пригласи", "можно", "вступить", ...), or a one-word guild invite ("ginv", "гинв"). Not when it is about the
// sender's own guild ("join my guild", "в мою гильдию") or a no ("don't want a guild", "не хочу в гильдию").
bool IsJoinRequest(std::string const& norm);

// A short yes or plain invite ask ("yes", "sure pls", "inv", "can i get an invite", "да", "давай", "инв"), up to six words. Counts only
// right after the bot spoke about its guild to this player, or posted a "/w me for invite" line in Trade.
bool IsShortYes(std::string const& norm);

} // namespace Playerbot::V2::GuildAsk
