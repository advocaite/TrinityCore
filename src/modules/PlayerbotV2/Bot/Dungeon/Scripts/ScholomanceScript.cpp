// ScholomanceScript — Scholomance (map 289, vanilla revamped 30-40).
// Necromancy-themed Western Plaguelands. Bosses: Darkmaster Gandling,
// Doctor Theolen Krastinov, Lady Illucia Barov, Lord Alexei Barov,
// Kirtonos the Herald, Ras Frostwhisper, etc.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Scholomance/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class ScholomanceScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 289; }
    char const* name() const override { return "scholomance"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            1853,   // Darkmaster Gandling: summoned once the other bosses are down
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            10498,   // Spectral Tutor
            10471,   // Scholomance Acolyte
            10477,   // Scholomance Necromancer
            10476,   // Scholomance Necrolyte
            10470,   // Scholomance Neophyte
            10469,   // Scholomance Adept
            11582,   // Scholomance Dark Summoner
        };
        a.mandatory_interrupt_spells = {
            // Darkmaster Gandling
            15790,  // Arcane Missiles
            12040,  // Shadow Shield
            18702,  // Curse
            17950,  // Shadow Portal
            // Lady Illucia Barov
            18671,  // Curse of Agony
            12542,  // Fear
            17234,  // Shadow Shock
            12528,  // Silence
            // Lord Alexei Barov
            17831,  // Call of Graves
            11672,  // Corruption
            10917,  // Flash Heal
            10929,  // Renew
            9889,   // Healing Touch
            // Ras Frostwhisper
            17228,  // Shadow Bolt
            12889,  // Curse of Tongues
            14515,  // Dominate Mind
            // Kirtonos the Herald
            18144,  // Swoop
            12882,  // Wing Flap
            8379,   // Disarm
            6016,   // Pierce Armor
            16467,  // Kirtonos Transform
            // Vectus
            18103,  // Backhand
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            17613,   // Dark Mending
            12739,   // Shadow Bolt
            14887,   // Shadow Bolt Volley
            15043,   // Frostbolt
            17615,   // Mana Burn
            11443,   // Cripple
        };
        a.cc_priority_entries = {
            10469,   // Scholomance Adept
            10470,   // Scholomance Neophyte
            10471,   // Scholomance Acolyte
            10477,   // Scholomance Necromancer
            10472,   // Scholomance Occultist
            10476,   // Scholomance Necrolyte
        };
        a.dangerous_auras = {
            // Generic Scholo
            8269,   // Frenzy
            16509,  // Rend
            // Lord Alexei
            17773,  // Illusion
            // The Ravenian
            24673,  // Curse of Blood
        };
        // Boss progression — Scholomance has ~7-8 encounters
        // (some are random spawns / Cata revamp differs from classic).
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            10901,  // Lorekeeper Polkelt
            10503,  // Jandice Barov
            11261,  // Doctor Theolen Krastinov
            10504,  // Lord Alexei Barov
            10502,  // Lady Illucia Barov
            10507,  // The Ravenian
            10432,  // Vectus
            10508,  // Ras Frostwhisper
        };
        // Progression waypoints — Scholo (classic) is multi-floor with
        // Gandling teleports bots to side rooms throughout the run.
        // Bots default to ground-floor sequential path; teleports are
        // handled by snapshot's nearby_enemies detection.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 289), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { 274.9f, 1.3f, 85.3f },   // Lorekeeper Polkelt
            { 268.2f, 73.6f, 95.9f },   // Jandice Barov
            { 182.2f, -95.4f, 85.3f },   // Doctor Theolen Krastinov
            { 178.7f, -91.0f, 70.9f },   // Lord Alexei Barov
            { 266.0f, 0.9f, 75.3f },   // Lady Illucia Barov
            { 103.3f, -1.7f, 75.2f },   // The Ravenian
            { 143.5f, 99.1f, 104.7f },   // Vectus
            { -25.1f, 141.3f, 83.9f },   // Ras Frostwhisper
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeScholomanceScript()
{
    return std::make_unique<ScholomanceScript>();
}

} // namespace Playerbot
