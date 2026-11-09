// DrinkSayRule - says "drinking" in party chat when a bot sits down to drink for a player (Bot/DrinkSay.h has the pure part).
#pragma once

#include "Bot/BotAI.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/DrinkSay.h"
#include "SharedDefines.h"

namespace Playerbot::Classic {

// Call where a drink item is used. Once per kDrinkSayQuietMs per bot: the ActionKind::OocFood lockout is 60 s and the key below is
// not an item entry, so it never blocks a real food retry.
inline void SayDrinkingIfDue(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, uint32 now_ms)
{
    constexpr uint64 kKey = 0xD81A7C0Full;
    bool const recent = ai.action_recently_tried(BotAI::ActionKind::OocFood, kKey, now_ms) || SameRest(ai.since_resting_ms(now_ms));
    if (!ShouldSayDrinking(s.is_led_by_player(), s.max_power(POWER_MANA) > 0, s.power_pct(POWER_MANA), recent))
        return;
    emit.say(DrinkSayText(s.guid().GetCounter()));
    ai.note_action_retry(BotAI::ActionKind::OocFood, kKey, now_ms);
}

} // namespace Playerbot::Classic
