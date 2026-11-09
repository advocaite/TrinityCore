// RazorfenDownsScript — Razorfen Downs (map 129, vanilla 32-43).
// Quilboar / undead. Bosses: Tuten'kash, Glutton, Mordresh Fire Eye,
// Amnennar the Coldbringer, Plaguemaw the Rotting, Ragglesnout.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/RazorfenDowns/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class RazorfenDownsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 129; }
    char const* name() const override { return "razorfen_downs"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            7355,   // Tuten'kash: comes out of the web when the gong is struck
            7356,   // Plaguemaw the Rotting: summoned by the Plaguemaw event
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7337,   // Death's Head Necromancer
            7342,   // Skeletal Summoner
            7341,   // Skeletal Frostweaver
            7335,   // Death's Head Geomancer
            7352,   // Frozen Soul
        };
        a.mandatory_interrupt_spells = {
            // Amnennar the Coldbringer
            13009,  // Amnennar's Wrath
            15530,  // Frostbolt
            15531,  // Frost Nova
            12642,  // Frost Spectres
            // Glutton
            12627,  // Disease Cloud
            // Mordresh Fire Eye
            12466,  // Fireball
            12470,  // Fire Nova
            // Tuten'kash
            8876,   // Thrash
            12252,  // Web Spray
            12254,  // Virulent Poison
            12255,  // Curse of Tuten'kash
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            11443,   // Cripple
            9613,   // Shadow Bolt
            9672,   // Frostbolt
            9053,   // Fireball
            8814,   // Flame Spike
            12258,   // Summon Shadowcaster
            12528,   // Silence
        };
        a.cc_priority_entries = {
            7337,   // Death's Head Necromancer
            7342,   // Skeletal Summoner
            7341,   // Skeletal Frostweaver
            7335,   // Death's Head Geomancer
        };
        a.dangerous_auras = {
            12795,  // Frenzy
        };
        // Boss progression — Razorfen Downs has 5 encounters.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. Tuten'kash and
        // Plaguemaw have no static spawn (event_summoned_bosses), so they are not on it; with them in, every waypoint answered the
        // wrong boss (bosses[0] Tuten'kash was steered to Glutton's spot, Glutton to Mordresh's).
        a.bosses = {
            8567,   // Glutton
            7357,   // Mordresh Fire Eye
            7358,   // Amnennar the Coldbringer (final)
            7354,   // Ragglesnout (rare side spawn)
        };
        // Progression waypoints — Razorfen Downs is a quilboar-undead
        // outdoor dungeon: spiral path up to Amnennar's throne.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 129), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { 2468.7f, 1006.8f, 23.8f },   // Glutton
            { 2466.6f, 671.4f, 63.5f },   // Mordresh Fire Eye
            { 2403.4f, 960.9f, 55.1f },   // Amnennar the Coldbringer
            { 2413.0f, 1025.9f, 37.9f },   // Ragglesnout
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRazorfenDownsScript()
{
    return std::make_unique<RazorfenDownsScript>();
}

} // namespace Playerbot
