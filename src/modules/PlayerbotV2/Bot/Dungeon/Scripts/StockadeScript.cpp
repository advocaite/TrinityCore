// StockadeScript — The Stockade (map 34, vanilla 22-30).
// Linear humanoid dungeon. Modern Cata-revamp bosses: Targorr the
// Dread, Hogger (final), Bruegal Ironknuckle, Inmate, Lord Overheat,
// Randolph Moloch.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/TheStockade/boss_*.cpp

#include "../DungeonScript.h"
#include "../../ClassTables.h"

namespace Playerbot {

namespace {

// The Stockade as Classic 1.60 has it: the Cata roster above has no spawns there, so the tank walked the Cata route and stepped
// into the void at (166.5,71.6) (RC10 stockade_targorr: stalled 120 s at (151.6,60.1), "NO TARGET ... script_bosses=3"). One spawn
// row per boss on map 34 in the live world DB (2026-10-30). The instance script keeps no state for these bosses, so a killed one
// is passed by the owed-boss waiver at its spot. Order: Targorr in the main hall, the south wing, then the north wing.
DungeonAdvice StockadeClassicAdvice()
{
    DungeonAdvice a;
    a.bosses = {
        1696,   // Targorr the Dread (main hall)
        1666,   // Kam Deepfury (south wing)
        1717,   // Hamhock (south wing)
        1716,   // Bazil Thredd (south wing, end)
        1720,   // Bruegal Ironknuckle (north wing, rare)
        1663,   // Dextren Ward (north wing, end)
    };
    a.progression_waypoints = {
        { 159.582f,    1.253f, -25.606f },   // Targorr the Dread
        { 142.644f,  -71.854f, -34.856f },   // Kam Deepfury
        { 105.523f, -105.795f, -35.106f },   // Hamhock
        {  94.483f, -125.829f, -33.940f },   // Bazil Thredd
        { 160.051f,   45.709f, -34.773f },   // Bruegal Ironknuckle
        { 166.789f,  134.787f, -33.940f },   // Dextren Ward
    };
    return a;
}

class StockadeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 34; }
    char const* name() const override { return "stockade"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        if (IsVanillaRealm())
            return StockadeClassicAdvice();
        DungeonAdvice a;
        a.mandatory_interrupt_spells = {
            // Hogger
            86604,  // Vicious Slice
            86620,  // Maddening Call
            // Lord Overheat
            12466,  // Fireball
            86633,  // Overheat
            86636,  // Rain of Fire
            // Randolph Moloch
            86726,  // Wildly Stabbing
            86729,  // Sweep
            55964,  // Vanish
            55966,  // Shadowstep
        };
        a.cc_priority_entries = { 1490 };
        a.dangerous_auras = {
            86736,  // Enrage (Hogger)
        };
        // Boss progression — DB-VERIFIED against wc_world creature spawns
        // on map 34 (2026-06-11). The previous lone entry 46916 ("verified
        // vs TC instance script") does NOT exist as a spawn in this world
        // DB — the boss-as-destination navigator had ZERO valid targets in
        // Stockades and the tank stalled whenever no trash was in scan
        // range. Encounter order matches the 3-encounter run tracker:
        //   46383 Randolph Moloch (172.5,  -2.2, -25.5) — main hall east
        //   46264 Lord Overheat   ( 99.5,-116.8, -35.1) — south wing
        //   46254 Hogger (final)  (158.0, 116.6, -35.1) — north wing
        a.bosses = {
            46383,  // Randolph Moloch
            46264,  // Lord Overheat
            46254,  // Hogger (final)
        };
        // Progression waypoints — anchored on actual creature spawn
        // coordinates from this DB (guaranteed on-mesh), replacing the
        // old "public coord dump approximations" whose z-level (-23.7)
        // matched no spawn in the instance. Route: central corridor
        // east to Randolph, then south wing down to Overheat, then
        // north wing to Hogger.
        a.progression_waypoints = {
            {  90.0f,    0.0f, -25.6f },   // central corridor west
            { 135.0f,    1.0f, -25.6f },   // corridor mid
            { 160.0f,   -1.0f, -25.5f },   // east end (Randolph Moloch)
            { 113.0f,  -60.0f, -34.9f },   // south wing stairs
            {  99.0f, -116.0f, -35.1f },   // Lord Overheat chamber
            { 132.0f,   69.0f, -33.9f },   // north wing
            { 158.0f,  116.0f, -35.1f },   // Hogger's cell
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStockadeScript()
{
    return std::make_unique<StockadeScript>();
}

} // namespace Playerbot
