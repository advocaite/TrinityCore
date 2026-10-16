// QuestSiteShare - how many world bots may work the same quest spot. Bots pick their quest objectives one by one, so a popular mob
// camp can draw a dozen of them to the same few mobs; the mobs are gone as fast as they spawn and the crowd looks wrong. The bots at
// a spot take slots in a fixed order (real players, then companions of players, then world bots by id); a world bot whose place is past
// the cap gives that objective up for a while and the quest picker takes another. Deterministic, so every bot reaches the same answer
// about who stays, with no shared counter. Tested in tests/game/ClassicQuestSiteShare.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Playerbot::V2::Fleet
{

struct SiteWorker
{
    std::uint64_t id = 0;
    bool ahead_of_bots = false;   // a real player, or the companion of one: always has its slot
};

// Must the world bot `my_id` give the spot up? `near` are the other workers within reach of it. Its place is one past everyone ahead
// of it, so it yields when that place exceeds `cap`. cap 0 = never.
inline bool YieldQuestSite(std::uint64_t my_id, std::vector<SiteWorker> const& near, std::uint32_t cap)
{
    if (cap == 0)
        return false;
    std::uint32_t ahead = 0;
    for (SiteWorker const& w : near)
        if (w.ahead_of_bots || w.id < my_id)
            ++ahead;
    return ahead >= cap;
}

constexpr std::uint32_t kQuestSiteCapDefault = 5u;           // workers within kQuestSiteRadius of a bot
constexpr float kQuestSiteRadius = 40.0f;                    // the snapshot's friend scan
constexpr std::uint32_t kQuestSiteGiveUpMs = 15u * 60u * 1000u;

} // namespace Playerbot::V2::Fleet
