// ShareMobs - the rule-side test of Fleet/PlayerWorkAreas.h: should this bot leave a mob, or a grind spot, to a real player?
// Only a bot of the world asks (no owner, not led by a player); a player's own companions fight beside him as before.
#pragma once

#include "Bot/BotSnapshotView.h"
#include "Bot/QuestObjectiveLocations.h"
#include "Fleet/PlayerWorkAreas.h"
#include "Services.h"
#include "Util/ConfigReader.h"

namespace Playerbot::V2::ShareMobs
{

inline bool Applies(BotSnapshotView const& s)
{
    return Services::Config().share_mobs_enable() && !s.has_owner_character() && !s.is_led_by_player();
}

inline WorkAreas::Settings SettingsNow()
{
    WorkAreas::Settings st;
    st.enabled = Services::Config().share_mobs_enable();
    st.camp_radius = Services::Config().share_mobs_camp_radius();
    st.quest_radius = Services::Config().share_mobs_quest_radius();
    return st;
}

// Leave this mob alone: a real player is working it (camp) or needs it for a quest while near.
inline bool LeaveMob(BotSnapshotView const& s, NearbyUnit const& u)
{
    if (!Applies(s))
        return false;
    auto const areas = WorkAreas::Board::Get().Current();
    if (!areas || areas->empty())
        return false;
    WorkAreas::Claim const c = WorkAreas::ClaimOf(*areas, SettingsNow(), s.map_id(), u.x, u.y, u.entry, s.group_guid().GetCounter(),
        [](std::uint32_t item, std::uint32_t entry) { return QuestItemDropsFromCreature(item, entry); });
    if (c == WorkAreas::Claim::Camp)
        WorkAreas::Board::Get().camp_skips.fetch_add(1, std::memory_order_relaxed);
    else if (c == WorkAreas::Claim::Quest)
        WorkAreas::Board::Get().quest_skips.fetch_add(1, std::memory_order_relaxed);
    return c != WorkAreas::Claim::None;
}

// Leave an elite or rare to the players: a bot of the world does not open on one (a quest boss with a long respawn was gone before the
// player who needed it arrived). It still fights one that attacks it, and a player's own bots fight beside him as before.
inline bool LeaveElite(BotSnapshotView const& s, NearbyUnit const& u)
{
    return u.is_elite_or_rare && Applies(s);
}

// LeaveElite for a mob that is not on this bot or its pet: one that attacks the bot is fought back, one that is fighting somebody else
// - a hunter taming a rare beast - is not the bot's to finish. For the pickers that look past the idle grind pick: the path-threat pull
// and the in-combat last-ditch target (fox4039, Discord 12 Nov: world bots kept killing the rare beasts hunters came to tame).
inline bool LeaveIdleElite(BotSnapshotView const& s, NearbyUnit const& u)
{
    if (!LeaveElite(s, u))
        return false;
    return u.victim != s.guid() && (s.pet_guid().IsEmpty() || u.victim != s.pet_guid());
}

// A grind spot a real player is standing in: pick another.
inline bool SpotTaken(BotSnapshotView const& s, float x, float y)
{
    if (!Applies(s))
        return false;
    auto const areas = WorkAreas::Board::Get().Current();
    if (!areas || areas->empty())
        return false;
    bool const taken = WorkAreas::PlayerNear(*areas, s.map_id(), x, y, Services::Config().share_mobs_camp_radius() * 2.0f,
                                             s.group_guid().GetCounter());
    if (taken)
        WorkAreas::Board::Get().spot_skips.fetch_add(1, std::memory_order_relaxed);
    return taken;
}

} // namespace Playerbot::V2::ShareMobs
