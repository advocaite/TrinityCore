// ExcavationSiteScript - Excavation Site: Wetlands (Forever, map 2998, levels 25-30).
// Encounters (DungeonEncounter.db2): Saltspine, Shadetooth, Highland Horror, Relic Guardian (last). The Relic Guardian sleeps
// uninteractible until someone uses the Archival Panel (classic_instance_excavation_site_wetlands.cpp).
//
// Without a script the tank had no boss to walk to: it cleared every pack it could see, walking north away from Saltspine
// (-3935,-2019) and never closer than about 100 y to him, until the priest was dry from 20 minutes of trash fights
// (exc_saltspine 0136 round 22: healer_oom at 939 s, goal not reached at 1200 s).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ExcavationSiteScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2998; }
    char const* name() const override { return "excavationsite"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Encounter order of the client data.
        a.bosses = {
            260322,   // Saltspine
            260325,   // Shadetooth
            260808,   // Highland Horror
        };
        // The Relic Guardian (260326) is left out: he cannot be attacked until the Archival Panel is used.
        // The waypoints are the boss spawns (world DB), index for index with the bosses above.
        a.progression_waypoints = {
            { -3935.1f, -2019.2f, 146.2f },   // Saltspine
            { -3826.9f, -1824.0f, 137.1f },   // Shadetooth
            { -3782.6f, -1634.0f, 134.0f },   // Highland Horror
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeExcavationSiteScript()
{
    return std::make_unique<ExcavationSiteScript>();
}

} // namespace Playerbot
