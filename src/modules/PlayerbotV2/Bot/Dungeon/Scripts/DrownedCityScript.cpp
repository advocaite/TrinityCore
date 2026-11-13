// DrownedCityScript - The Drowned City (Forever, map 2970, levels 35-40).
// Encounters (DungeonEncounter.db2): Zul'Alai, Var'Taka, Captain Dreadrise, Deathless Guardians (Deathless Marrow), Primeval
// Elemental, Min'loth the Serpent. The Primeval Elemental waits hidden and wakes when Deathless Marrow dies
// (classic_instance_the_drowned_city.cpp).
//
// The same gap as City of Dalaran and Excavation Site (0136 round 22): without a script the tank has no boss to walk to and
// only clears the trash it can see.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class DrownedCityScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2970; }
    char const* name() const override { return "drownedcity"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Encounter order of the client data. The live route between the bosses is not known (INFERRED).
        a.bosses = {
            258905,   // Zul'Alai
            258924,   // Var'Taka
            260042,   // Captain Dreadrise
            260202,   // Deathless Marrow
            263886,   // Primeval Elemental (wakes when Marrow dies)
            260243,   // Min'loth the Serpent
        };
        // The waypoints are the boss spawns (world DB), index for index with the bosses above.
        a.progression_waypoints = {
            { -13301.3f, 2464.0f, -1.6f },    // Zul'Alai
            { -13043.3f, 2761.8f, -27.4f },   // Var'Taka
            { -13230.8f, 2619.3f, -17.3f },   // Captain Dreadrise
            { -13601.4f, 2698.9f, -47.5f },   // Deathless Marrow
            { -13552.1f, 2719.5f, -33.2f },   // Primeval Elemental
            { -13320.0f, 3190.0f, 2.1f },     // Min'loth the Serpent
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDrownedCityScript()
{
    return std::make_unique<DrownedCityScript>();
}

} // namespace Playerbot
