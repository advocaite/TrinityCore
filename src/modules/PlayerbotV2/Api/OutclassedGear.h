// OutclassedGear - is a piece of gear in a bot's bags outclassed by what it wears (pure: unit tested).
//
// A bot keeps every soulbound piece it ever replaced: quest rewards, dungeon drops, the old weapon. They cannot be sold at the
// auction house or traded, the equip rule never puts them back on, and they fill the bags until the bot cannot loot. A vendor
// visit (API::sell_trash, IsBagRot) sells such a piece only when it is outclassed in every slot it could go in: each of those
// slots holds a piece of the same kind that scores higher for the bot's class and spec by a clear margin and is at least its item
// level. An empty slot, a slot holding something else (a two-hander against a one-hander and an off-hand) or a close call keeps it.

#pragma once

#include <cstddef>
#include <cstdint>

namespace Playerbot::Gear {

// What a slot the bag piece could go in holds now.
struct WornInSlot
{
    bool     empty = true;
    bool     same_kind = false;   // comparable with the bag piece (not a two-hander against a one-hander)
    int32_t  score = 0;           // ScoreItemForBot of the worn piece
    uint32_t item_level = 0;      // its item level for the bot
};

// The worn piece must beat the bag piece by this share of its own score (and by at least kOutclassMinPoints) to count.
constexpr int32_t kOutclassPercent = 5;
constexpr int32_t kOutclassMinPoints = 2;
// ScoreItemForBot answers this or less for a piece the bot cannot wear at all (that case is IsBagRot's own branch).
constexpr int32_t kCannotWearScore = -1000;

inline bool IsOutclassed(int32_t bag_score, uint32_t bag_item_level, WornInSlot const* slots, size_t n)
{
    if (!slots || n == 0 || bag_score <= kCannotWearScore)
        return false;
    for (size_t i = 0; i < n; ++i)
    {
        WornInSlot const& w = slots[i];
        if (w.empty || !w.same_kind)
            return false;
        int32_t const margin = w.score > 0 ? (w.score * kOutclassPercent) / 100 : 0;
        if (w.score < bag_score + (margin > kOutclassMinPoints ? margin : kOutclassMinPoints))
            return false;
        if (w.item_level < bag_item_level)
            return false;
    }
    return true;
}

} // namespace Playerbot::Gear
