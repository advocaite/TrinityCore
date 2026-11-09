// StratholmeScript — Stratholme (map 329, vanilla 55-60).
// Two wings: Live (Scarlet) and Undead (Baron). Major bosses:
// Magistrate Barthilas, Cannon Master Willey, Postmaster Malown,
// Baroness Anastari, Ramstein the Gorger, Baron Rivendare, etc.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/Stratholme/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class StratholmeScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 329; }
    char const* name() const override { return "stratholme"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            10516,   // The Unforgiven: summoned by the Crimson / courtyard event
            10439,   // Ramstein the Gorger: summoned after the Postmaster event
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            10398,   // Thuzadin Shadowcaster
            10400,   // Thuzadin Necromancer
            10423,   // Crimson Priest
            10420,   // Crimson Initiate
            10419,   // Crimson Conjuror
            10422,   // Crimson Sorcerer
            10426,   // Crimson Inquisitor
            10425,   // Crimson Battle Mage
            10463,   // Shrieking Banshee
        };
        a.mandatory_interrupt_spells = {
            // Baron Rivendare
            17393,  // Shadowbolt
            15708,  // Mortal Strike
            17473,  // Raise Dead
            // Baroness Anastari
            16565,  // Banshee Wail
            16867,  // Banshee Curse
            18327,  // Silence
            17244,  // Possess
            // Balnazzar
            17286,  // Crusader's Hammer (AOE stun)
            17281,  // Crusader Strike
            17284,  // Holy Strike
            17399,  // Shadow Shock
            17287,  // Mind Blast
            13704,  // Psychic Scream
            12098,  // Sleep
            15690,  // Mind Control
            // Maleki the Pallid
            17503,  // Frostbolt
            20743,  // Drain Life
            17243,  // Drain Mana
            16869,  // Ice Tomb
            // Hearthsinger Forresten
            // Magistrate Barthilas (Cannon Master Willey trash)
            10101,  // Knock Away
            15615,  // Pummel
            // Nerub'enkan
            4962,   // Encasing Webs
            6016,   // Pierce Armor
            31602,  // Crypt Scarabs
            17235,  // Raise Undead Scarab
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            15586,   // Heal
            17138,   // Flash Heal
            15238,   // Holy Smite
            17142,   // Holy Fire
            13323,   // Polymorph
            12675,   // Frostbolt
            15232,   // Shadow Bolt
            11443,   // Cripple
            16868,   // Banshee Wail
            15493,   // Holy Light
            15785,   // Mana Burn
        };
        a.cc_priority_entries = {
            10419,   // Crimson Conjuror
            10420,   // Crimson Initiate
            10422,   // Crimson Sorcerer
            10423,   // Crimson Priest
            10426,   // Crimson Inquisitor
            10425,   // Crimson Battle Mage
            10398,   // Thuzadin Shadowcaster
            10400,   // Thuzadin Necromancer
        };
        a.dangerous_auras = {
            // Postmaster Malown adds
            16793,  // Draining Blow
            10887,  // Crowd Pummel
            14099,  // Mighty Blow
            16791,  // Furious Anger
            // Cannonmaster
            16496,  // Shoot
            // Baron + adds
            17467,  // Unholy Aura
            17246,  // Possessed (Anastari debuff)
            // Ramstein
            5568,   // Trample
            17307,  // Knockout
            // Timmy
            17470,  // Ravenous Claw
            // Stratholme courier
            7713,   // Wailing Dead
            6253,   // Backhand
            8552,   // Curse of Weakness
            12889,  // Curse of Tongues
            17831,  // Call of the Grave
        };
        // Boss progression — NPC entries from TC's stratholme.h.
        // Stratholme has two wings (Scarlet live + Undead Baron); the
        // tank-advance scan finds whichever boss is closest as the
        // bot moves through the dungeon.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            10558,  // Hearthsinger Forresten
            11032,  // Commander Malor
            10811,  // Instructor Galford
            10440,  // Baron Rivendare (final, Undead wing)
        };
        // Progression waypoints — Strat is two wings sharing one map_id.
        // Bots default to the Undead Baron path (more frequently run for
        // Baron's mount). Scarlet wing falls back to boss-Cell scan from
        // the Service Entrance. Coords approximate from public dumps.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 329), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { 3595.8f, -3509.9f, 137.5f },   // Hearthsinger Forresten
            { 3621.1f, -3126.8f, 135.7f },   // Malor the Zealous
            { 3456.0f, -3103.4f, 136.5f },   // Archivist Galford
            { 4032.8f, -3333.0f, 115.1f },   // Baron Rivendare
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeStratholmeScript()
{
    return std::make_unique<StratholmeScript>();
}

} // namespace Playerbot
