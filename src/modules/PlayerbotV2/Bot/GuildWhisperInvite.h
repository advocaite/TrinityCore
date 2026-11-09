// GuildWhisperInvite - a player whispers a bot "can I join your guild" / "пригласи в гильдию" and gets a normal guild invite.
//
// Works whatever PlayerbotV2.Guilds.InvitePlayers says: that setting is about bots inviting players who did not ask; here the
// player asked. The invite comes from the whispered bot when it is the guild master or an officer, else from an online officer
// bot of the same guild (the player is told who). Only bot-run guilds. When no invite can be sent the bot says why, and the
// player's language applies as for every whisper (Api/BotLanguage, Russian entries in Util/BotLocaleRu.cpp).
//
// A short "yes" / "inv" / "да" counts too, but only right after the bot talked about its guild to that player (3 minutes), or
// after it posted its "/w <name> for invite" line in Trade (30 minutes, anyone).
//
// World thread (the whisper hook: CMSG_CHAT_MESSAGE_WHISPER and the add-on whisper are thread-unsafe handlers).

#pragma once

#include "Define.h"

#include <string>

class Player;

namespace Playerbot::V2::GuildWhisperInvite {

// A line that plainly asks for a guild invite. Runs before the owner command parser. True when the line was taken (answered).
bool TryRequest(Player* sender, Player* bot, std::string const& msg);

// A short yes after an offer. Runs after the command parser did not take the line. True when taken.
bool TryFollowUp(Player* sender, Player* bot, std::string const& msg);

// The bot just talked about its guild to `player_low` (0 = posted in Trade: any player for 30 minutes).
void NoteOffer(uint64 bot_low, uint64 player_low);

} // namespace Playerbot::V2::GuildWhisperInvite
