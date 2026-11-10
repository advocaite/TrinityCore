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

#include "classic_script_text.h"
#include "ChatTextBuilder.h"
#include "DatabaseEnv.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "Player.h"
#include "Unit.h"
#include <mutex>
#include <unordered_map>

namespace
{
// VMaNGOS ChatType
enum ClassicChatType : uint8
{
    CLASSIC_CHAT_SAY          = 0,
    CLASSIC_CHAT_YELL         = 1,
    CLASSIC_CHAT_TEXT_EMOTE   = 2,
    CLASSIC_CHAT_BOSS_EMOTE   = 3,
    CLASSIC_CHAT_WHISPER      = 4,
    CLASSIC_CHAT_BOSS_WHISPER = 5,
    CLASSIC_CHAT_ZONE_YELL    = 6,
    CLASSIC_CHAT_ZONE_EMOTE   = 7
};

std::unordered_map<uint32, uint8> const& ChatTypes()
{
    static std::unordered_map<uint32, uint8> types;
    static std::once_flag loaded;
    std::call_once(loaded, []()
    {
        if (QueryResult result = WorldDatabase.Query("SELECT ID, ChatType FROM classic_broadcast_text_type"))
        {
            do
            {
                Field* fields = result->Fetch();
                types[fields[0].GetUInt32()] = fields[1].GetUInt8();
            } while (result->NextRow());
        }
    });
    return types;
}

// VMaNGOS zone yells / zone emotes reach every player of the source's zone on its map
void ZoneTalk(Unit* unit, uint32 textId, ChatMsg msgType, Unit* target)
{
    Trinity::BroadcastTextBuilder builder(unit, msgType, textId, unit->GetGender(), target);
    Trinity::LocalizedDo<Trinity::BroadcastTextBuilder> localizer(builder);
    uint32 const zoneId = unit->GetZoneId();
    for (MapReference const& ref : unit->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->GetZoneId() == zoneId)
                localizer(player);
}
}

void ClassicScriptText(uint32 broadcastTextId, WorldObject* source, Unit* target)
{
    Unit* unit = source ? source->ToUnit() : nullptr;
    if (!unit || !broadcastTextId)
        return;

    auto const& types = ChatTypes();
    auto itr = types.find(broadcastTextId);
    uint8 type = itr != types.end() ? itr->second : CLASSIC_CHAT_SAY;

    switch (type)
    {
        case CLASSIC_CHAT_YELL:
            unit->Yell(broadcastTextId, target);
            break;
        case CLASSIC_CHAT_ZONE_YELL:
            ZoneTalk(unit, broadcastTextId, CHAT_MSG_MONSTER_YELL, target);
            break;
        case CLASSIC_CHAT_TEXT_EMOTE:
            unit->TextEmote(broadcastTextId, target);
            break;
        case CLASSIC_CHAT_ZONE_EMOTE:
            ZoneTalk(unit, broadcastTextId, CHAT_MSG_MONSTER_EMOTE, target);
            break;
        case CLASSIC_CHAT_BOSS_EMOTE:
            unit->TextEmote(broadcastTextId, target, true);
            break;
        case CLASSIC_CHAT_WHISPER:
        case CLASSIC_CHAT_BOSS_WHISPER:
            if (Player* player = target ? target->ToPlayer() : nullptr)
                unit->Whisper(broadcastTextId, player, type == CLASSIC_CHAT_BOSS_WHISPER);
            break;
        default:
            unit->Say(broadcastTextId, target);
            break;
    }
}

void ClassicScriptZoneEmote(uint32 broadcastTextId, WorldObject* source, Unit* target)
{
    Unit* unit = source ? source->ToUnit() : nullptr;
    if (!unit || !broadcastTextId)
        return;

    ZoneTalk(unit, broadcastTextId, CHAT_MSG_MONSTER_EMOTE, target);
}
