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

#include "AuctionHouseBotVanilla.h"
#include "Config.h"
#include "ItemTemplate.h"
#include "SharedDefines.h"

namespace AuctionBotVanilla
{
namespace
{
    bool _enabled = false;
    int32 _maxReqLevel = 60;
    uint32 _maxItemLevel = 100;   // Naxxramas epics top out around 92
}

void LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("AuctionHouseBot.Vanilla.Only", false);
    _maxReqLevel = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.MaxReqLevel", 60);
    _maxItemLevel = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.MaxItemLevel", 100);
}

bool Enabled()
{
    return _enabled;
}

bool IsAllowed(ItemTemplate const* proto)
{
    if (!_enabled)
        return true;

    // anything flagged for a later expansion
    if (proto->GetRequiredExpansion() > 0)
        return false;

    // no gems, glyphs or other post-classic classes
    switch (proto->GetClass())
    {
        case ITEM_CLASS_GEM:
        case ITEM_CLASS_GLYPH:
            return false;
        default:
            break;
    }

    // artifact/legendary and above are never sold by the bot
    if (proto->GetQuality() > ITEM_QUALITY_EPIC)
        return false;

    if (proto->GetBaseRequiredLevel() > _maxReqLevel)
        return false;

    if (proto->GetBaseItemLevel() > _maxItemLevel)
        return false;

    return true;
}
}
