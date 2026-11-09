/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLASSIC_SCRIPT_TEXT_H
#define CLASSIC_SCRIPT_TEXT_H

#include "Define.h"

class Unit;
class WorldObject;

// Classic 1.60 ports of VMaNGOS (ScriptDev2) scripts.
// VMaNGOS DoScriptText(textId, source, target): textId is a BroadcastText id; the chat type (say, yell, emote, whisper, ...)
// comes from world.classic_broadcast_text_type (imported from VMaNGOS broadcast_text.chat_type).
void ClassicScriptText(uint32 broadcastTextId, WorldObject* source, Unit* target = nullptr);

// VMaNGOS DoScriptText(textId, source, target, CHAT_TYPE_ZONE_EMOTE): the script overrides the text's own chat type.
void ClassicScriptZoneEmote(uint32 broadcastTextId, WorldObject* source, Unit* target = nullptr);

#endif
