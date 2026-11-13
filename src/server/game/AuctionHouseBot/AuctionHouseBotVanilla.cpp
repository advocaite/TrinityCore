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
#include <algorithm>
#include <array>

namespace AuctionBotVanilla
{
namespace
{
    bool _enabled = false;
    int32 _maxReqLevel = 60;
    uint32 _maxItemLevel = 100;   // Naxxramas epics top out around 92
    bool _valueCurve = false;

    // copper per item level squared, by quality (gray .. epic); AuctionHouseBot.Vanilla.ValueCurve.Gray .. .Epic
    std::array<uint32, 5> QualityFactor = { 1, 2, 15, 50, 150 };
    // AuctionHouseBot.Vanilla.ValueCurve.Scale: one easy knob over the whole curve (1 = as set, 2 = double, 0.5 = half)
    float _scale = 1.0f;
    // AuctionHouseBot.Vanilla.ValueCurve.FullValueItemLevel: below this item level the curve scales down in proportion
    // (item level / this), so low-level goods are not priced like endgame ones (0 = off)
    uint32 _fullValueItemLevel = 0;
}

void LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("AuctionHouseBot.Vanilla.Only", false);
    _maxReqLevel = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.MaxReqLevel", 60);
    _maxItemLevel = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.MaxItemLevel", 100);
    _valueCurve = sConfigMgr->GetBoolDefault("AuctionHouseBot.Vanilla.ValueCurve", false);
    QualityFactor[0] = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.Gray", 1);
    QualityFactor[1] = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.White", 2);
    QualityFactor[2] = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.Green", 15);
    QualityFactor[3] = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.Blue", 50);
    QualityFactor[4] = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.Epic", 150);
    _scale = std::clamp(sConfigMgr->GetFloatDefault("AuctionHouseBot.Vanilla.ValueCurve.Scale", 1.0f), 0.1f, 10.0f);
    _fullValueItemLevel = sConfigMgr->GetIntDefault("AuctionHouseBot.Vanilla.ValueCurve.FullValueItemLevel", 0);
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

uint64 UnitValue(ItemTemplate const* proto)
{
    if (!_valueCurve)
        return 0;

    // the 70532 data gives most loot-only goods a token vendor price (enchanting essences and shards: 1 copper),
    // so the vendor price alone lists them for a few copper
    uint64 vendor = std::max<uint64>(proto->GetBuyPrice() / std::max<uint32>(1, proto->GetBuyCount()), uint64(proto->GetSellPrice()) * 4);
    if (proto->GetClass() == ITEM_CLASS_PROJECTILE)
        vendor = proto->GetBuyPrice() / 200;   // ammo is bought in stacks of 200

    uint32 classPct = 0;
    switch (proto->GetClass())
    {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
        case ITEM_CLASS_CONTAINER:
        case ITEM_CLASS_TRADE_GOODS:
            classPct = 100;
            break;
        case ITEM_CLASS_RECIPE:
        case ITEM_CLASS_MISCELLANEOUS:
            classPct = 50;
            break;
        case ITEM_CLASS_CONSUMABLE:
            classPct = 25;
            break;
        default:                        // ammo, reagents, quivers: vendor goods, vendor price
            break;
    }

    uint32 quality = std::min<uint32>(proto->GetQuality(), QualityFactor.size() - 1);
    uint64 itemLevel = proto->GetBaseItemLevel();
    double curve = double(itemLevel * itemLevel * QualityFactor[quality] * classPct / 100) * _scale;
    // Squared item level alone priced a level 14 green bracer at 1g (Discord 2026-11-12, dumpins); vanilla houses sold them for
    // a few tens of silver. Below the full value level the price falls with the item level once more.
    if (_fullValueItemLevel && itemLevel < _fullValueItemLevel)
        curve *= double(itemLevel) / double(_fullValueItemLevel);
    return std::max<uint64>(vendor, uint64(curve));
}
}
