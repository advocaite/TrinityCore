// CityOfDalaranScript - City of Dalaran (Forever, map 2959, levels 28-33).
// The Sewer Grate lands the party at the sewer start (310,384). Two sewer bosses first (Atrexis, then Lyn below him); the door to
// the city opens once both are dead (classic_instance_city_of_dalaran.cpp). In the city the three square bosses (Arcane Anomaly,
// Arcanic Enigma, Unstable Sentinel) open the Prismatic Doors to Fel Ancient, and Mana Devourer plus Fel Ancient open the portal
// up to the Shade of the Archmage.
//
// Without a script the tank had no boss to walk to: once Atrexis was dead it cleared what trash it could see, then stood at
// (249,278) with nothing in its 150 y scan while Lyn waited 105 y south (cod_lyn 0136 round 22: [tank_adv_diag] NO TARGET
// script_bosses=0 wp=0/0, then 700 s of wandering round (320,300), goal not reached).

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class CityOfDalaranScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 2959; }
    char const* name() const override { return "cityofdalaran"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        // Encounter order of the instance script, which is also the order the doors allow. The order of the three city squares
        // among themselves is INFERRED (no live route data); any order opens the doors.
        a.bosses = {
            247126,   // Atrexis the Grave Knight (sewers)
            247032,   // Lyn the Ignored (sewers, below Atrexis)
            245999,   // Arcane Anomaly (city square)
            246016,   // Arcanic Enigma (city square)
            246017,   // Unstable Sentinel (city square)
            246008,   // Mana Devourer
            246003,   // Fel Ancient (behind the Prismatic Doors)
        };
        // Shade of the Archmage (246020) is left out: he stands in the Violet Chamber, which only the scripted portal reaches.
        // The waypoints are the boss spawns (world DB), index for index with the bosses above.
        a.progression_waypoints = {
            { 235.0f, 313.0f, 58.6f },    // Atrexis the Grave Knight
            { 226.9f, 175.7f, 59.3f },    // Lyn the Ignored
            { 71.5f, 224.6f, 108.1f },    // Arcane Anomaly
            { 346.9f, 182.8f, 89.6f },    // Arcanic Enigma
            { 362.9f, 263.2f, 88.3f },    // Unstable Sentinel
            { 93.9f, 353.7f, 98.6f },     // Mana Devourer
            { 240.5f, 310.7f, 96.4f },    // Fel Ancient
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeCityOfDalaranScript()
{
    return std::make_unique<CityOfDalaranScript>();
}

} // namespace Playerbot
