// DrinkSay - the line a mana user says in party chat when it sits down to drink, so the player leading the group knows why the group is
// waiting (a real player says "drinking"). Pure: the rule files call it and tests/game/ClassicDrinkSay.cpp reads the same numbers.
#pragma once

#include <cstdint>

namespace Playerbot::Classic {

// A drink that starts below this mana is worth announcing; a top-up near full is not.
constexpr int32_t kDrinkSayBelowManaPct = 80;
// Quiet time per bot between two lines (ms). The drink aura of one item lasts about 20-30 s, a full recovery several items.
constexpr uint32_t kDrinkSayQuietMs = 60000;

// Once per rest (the owner, 2026-10-29: Gun and Elentinari said "drinking" over and over at Thorium Point): a drink within this long of
// the bot last eating or drinking belongs to the same rest (the next item of a long drink, or a sit again after a step) and says nothing.
constexpr uint32_t kDrinkSayRestGapMs = 30000;

constexpr bool SameRest(uint32_t since_resting_ms)
{
    return since_resting_ms < kDrinkSayRestGapMs;
}

// `player_led`: the bot is in a group with a player who leads it. `mana_user`: it has a mana pool.
constexpr bool ShouldSayDrinking(bool player_led, bool mana_user, int32_t mana_pct, bool said_recently)
{
    return player_led && mana_user && mana_pct < kDrinkSayBelowManaPct && !said_recently;
}

// Three plain variants, one per bot (the same bot always says the same, so a group does not all say one line at once).
constexpr char const* DrinkSayText(uint64_t bot_guid)
{
    switch (bot_guid % 3)
    {
        case 0:  return "drinking";
        case 1:  return "drinking, one moment";
        default: return "need to drink";
    }
}

} // namespace Playerbot::Classic
