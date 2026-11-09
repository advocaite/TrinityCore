// ZulFarrakScript — Zul'Farrak (map 209, vanilla 41-51).
// Troll dungeon in Tanaris. Bosses: Antu'sul, Theka the Martyr,
// Sergeant Bly, Hydromancer Velratha, Sandfury Executioner,
// Witch Doctor Zum'rah, Chief Ukorz Sandscalp, Nekrum Gutchewer,
// Ruuzlu, Gahz'rilla.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/Kalimdor/ZulFarrak/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ZulFarrakScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 209; }
    char const* name() const override { return "zul_farrak"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            7796,   // Nekrum Gutchewer: Sergeant Bly event
            7273,   // Gahz'rilla: summoned with the Mallet of Zul'Farrak
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            5650,   // Sandfury Witch Doctor
            5648,   // Sandfury Shadowcaster
            7246,   // Sandfury Shadowhunter
        };
        a.mandatory_interrupt_spells = {
            // Witch Doctor Zum'rah
            12739,  // Shadow Bolt
            15245,  // Shadowbolt Volley
            11086,  // Ward of Zum'rah
            12491,  // Healing Wave
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            17843,   // Flash Heal
            12471,   // Shadow Bolt
            11641,   // Hex
        };
        a.cc_priority_entries = {
            5650,   // Sandfury Witch Doctor
            5648,   // Sandfury Shadowcaster
        };
        a.dangerous_auras = {
        };
        // Boss progression — Zul'Farrak has 7+ encounters.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            8127,   // Antu'sul
            7272,   // Theka the Martyr
            7271,   // Witch Doctor Zum'rah
            7604,   // Sergeant Bly (Pyramid event lead)
            7795,   // Hydromancer Velratha
            7797,   // Ruuzlu (gladiator boss)
            7267,   // Chief Ukorz Sandscalp (final)
        };
        // Progression waypoints — Zul'Farrak is an outdoor desert
        // dungeon (no roof in most sections). Linear walk: entry →
        // Antu'sul → west wing (Theka, Zum'rah) → pyramid stairs →
        // pyramid event (Bly's crew) → Velratha pool → Ruuzlu arena →
        // Chief Ukorz final platform.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 209), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { 1815.8f, 670.4f, 15.0f },   // Antu'sul
            { 1778.0f, 859.7f, 8.9f },   // Theka the Martyr
            { 1912.2f, 1016.1f, 11.6f },   // Witch Doctor Zum'rah
            { 1882.9f, 1299.3f, 48.4f },   // Sergeant Bly
            { 1698.2f, 1210.7f, 9.4f },   // Hydromancer Velratha
            { 1730.3f, 1015.2f, 54.9f },   // Ruuzlu
            { 1727.5f, 1017.4f, 54.9f },   // Chief Ukorz Sandscalp
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeZulFarrakScript()
{
    return std::make_unique<ZulFarrakScript>();
}

} // namespace Playerbot
