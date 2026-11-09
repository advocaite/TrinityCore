// GnomereganScript — Gnomeregan (map 90, vanilla 24-34).
// Mechanical / Troggs / Leper Gnome dungeon. Notable:
//   * Mekgineer Thermaplugg final fight: bombs from machinist
//     vendors + adds spawn from Walking Bombs. Adds (entry 7395
//     "Walking Bomb") must be priority killed before they detonate.
//   * Crowd Pummeler 9-60 — fires Pummel (interruptible, knockback).
//   * Grubbis pulls Viscous Fallout — single-target tank-and-spank
//     after first phase clears.
//   * Snipe (Crowd Pummeler) — interruptible ranged nuke.

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class GnomereganScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 90; }
    char const* name() const override { return "gnomeregan"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            7361,   // Grubbis: spawned by the Blastmaster Emi Shortfuse event
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            7915,   // Walking Bomb
            6392,   // Holdout Medic
            6220,   // Irradiated Horror
        };
        a.mandatory_interrupt_spells = {
            12555,  // Pummel (Crowd Pummeler)
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            22167,   // Heal
            8211,   // Chain Burn
            9771,   // Radiation Bolt
            11085,   // Chain Bolt
            9053,   // Fireball
        };
        // (12241 was listed as the bomb explosion; it is Twin Colossals Teleport, so nothing is listed.)
        // Radiation (9798) from the irradiated mobs and the Radiation Cloud (10341) of Gnomeregan's leaking tunnels: carriers keep clear of the group.
        a.dangerous_auras = { 9798, 10341 };
        // Boss progression — Gnomeregan has multiple bosses.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. Grubbis has no static
        // spawn (event_summoned_bosses) and no waypoint, so he is not on it; with him first, every waypoint answered the boss before it
        // (0.1.27 set A: the tank walked to Viscous Fallout's spot for Grubbis, waived him after 8 s, then was sent to the
        // Electrocutioner's spot for Viscous Fallout).
        a.bosses = {
            7079,   // Viscous Fallout
            6235,   // Electrocutioner 6000
            6229,   // Crowd Pummeler 9-60
            7800,   // Mekgineer Thermaplugg (final)
            6228,   // Dark Iron Ambassador (rare bonus, entry varies)
        };
        // Progression waypoints — Gnomeregan is a multi-level mechanical
        // dungeon. Path: entry tram → workshop → Grubbis pit → toxic
        // floor (Viscous Fallout) → engineering → Electrocutioner →
        // arena (Crowd Pummeler) → Thermaplugg's final stage. Some
        // verticality; Detour will reject any off-mesh hops harmlessly.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 90), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { -430.2f, 42.5f, -208.7f },   // Viscous Fallout
            { -552.0f, 502.9f, -216.7f },   // Electrocutioner 6000
            { -890.9f, 360.9f, -272.6f },   // Crowd Pummeler 9-60
            { -531.3f, 670.2f, -325.2f },   // Mekgineer Thermaplugg
            { -718.1f, 565.2f, -289.1f },   // Dark Iron Ambassador
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeGnomereganScript()
{
    return std::make_unique<GnomereganScript>();
}

} // namespace Playerbot
