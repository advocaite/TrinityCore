// KroldokStrongholdScript - Krol'dok Stronghold (Forever, map 3039, levels 40-45).
// Encounters (DungeonEncounter.db2): Slavecrusher Grox, Twilight Commander Zarmok, Handler Ashka, Nar'gurk, Overlord Mugg.
// Bossman Toof (264773) guards the way to Mugg but has no encounter (classic_instance_kroldok_stronghold.cpp), so he is not a
// boss here: the boss list is counted against the instance's encounters.
//
// The same gap as City of Dalaran and Excavation Site (0136 round 22): without a script the tank has no boss to walk to and
// only clears the trash it can see.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class KroldokStrongholdScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 3039; }
    char const* name() const override { return "kroldokstronghold"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Encounter order of the instance script. The live route between the bosses is not known (INFERRED).
        a.bosses = {
            258966,   // Slavecrusher Grox
            258972,   // Twilight Commander Zarmok
            259281,   // Handler Ashka
            258964,   // Nar'gurk
            258968,   // Overlord Mugg
        };
        // The waypoints are the boss spawns (world DB), index for index with the bosses above.
        a.progression_waypoints = {
            { -7455.6f, -3769.8f, 146.4f },   // Slavecrusher Grox
            { -7370.9f, -3985.6f, 166.8f },   // Twilight Commander Zarmok
            { -7544.9f, -4177.6f, 157.2f },   // Handler Ashka
            { -7367.2f, -4323.9f, 168.6f },   // Nar'gurk
            { -7328.9f, -4008.9f, 269.1f },   // Overlord Mugg
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeKroldokStrongholdScript()
{
    return std::make_unique<KroldokStrongholdScript>();
}

} // namespace Playerbot
