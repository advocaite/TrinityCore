// SunkenTempleScript — Temple of Atal'Hakkar / Sunken Temple
// (map 109, vanilla 50-60). Two-tier troll temple.
//   * Avatar of Hakkar event — 6 Atal'ai Defenders summon Avatar.
//     Adds priority kill.
//   * Jammal'an the Prophet + Ogom the Wretched — Jammal'an heals
//     Ogom; interrupt the heal.
//   * Hakkari Bloodkeepers / Priests — caster trash, CC priority.
//   * Shade of Eranikus (final tier) — Sleep aura debuff.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class SunkenTempleScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 109; }
    char const* name() const override { return "sunken_temple"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            8443,   // Avatar of Hakkar: summoned by the Atal'ai braziers
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            5273,   // Atal'ai High Priest
            5259,   // Atal'ai Witch Doctor
            5271,   // Atal'ai Deathwalker
            5291,   // Hakkari Frostwing
        };
        a.mandatory_interrupt_spells = {
            12039,  // Holy Light (Jammal'an heals Ogom)
            12559,  // Mind Blast (Hakkari Priest)
            12544,  // Frost Armor (Hakkari)
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            11986,   // Healing Wave
            12058,   // Chain Lightning
            9532,   // Lightning Bolt
            12096,   // Fear
            8398,   // Frostbolt Volley
            9591,   // Acid Spit
            12151,   // Summon Atal'ai Skeleton
        };
        a.cc_priority_entries = {
            5259,   // Atal'ai Witch Doctor
            5271,   // Atal'ai Deathwalker
            5273,   // Atal'ai High Priest
        };
        a.dangerous_auras = {
            12480,  // Sleep (Shade of Eranikus) — interrupted by damage
        };
        // Boss progression — NPC entries from TC's sunken_temple.h.
        // ST is a 2-tier troll dungeon; the dragonkin tier (5719-5722)
        // is fought sequentially before Jammal'an at the bottom and
        // Hakkar/Eranikus at the apex.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            5719,   // Morphaz (Green Dragonkin)
            5720,   // Weaver
            5721,   // Dreamscythe
            5722,   // Hazzas
            5710,   // Jammal'an the Prophet
            5709,   // Shade of Eranikus (final)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 109), in encounter order. Weaver (5720), Dreamscythe (5721)
        // and the Avatar of Hakkar (8443) are script-spawned with no
        // creature rows, so they contribute no waypoint; the pathfinder
        // routes the corridors between the remaining anchors.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 109), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { -671.6f, 73.3f, -90.8f },   // Morphaz
            { -425.1f, 105.8f, -90.8f },   // Weaver
            { -508.8f, 85.1f, -90.9f },   // Dreamscythe
            { -670.1f, 68.7f, -90.8f },   // Hazzas
            { -426.7f, -85.7f, -88.1f },   // Jammal'an the Prophet
            { -658.4f, -35.8f, -90.8f },   // Shade of Eranikus
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeSunkenTempleScript()
{
    return std::make_unique<SunkenTempleScript>();
}

} // namespace Playerbot
