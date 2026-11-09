// LowerBlackrockSpireScript — LBRS (map 229, vanilla 55-60).
// Bosses: Highlord Omokk, Shadow Hunter Vosh'gajin, War Master Voone,
// Mother Smolderweb, Urok Doomhowl, Quartermaster Zigris, Halycon,
// Gizrul the Slavener, Overlord Wyrmthalak.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/BlackrockSpire/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class LowerBlackrockSpireScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 229; }
    char const* name() const override { return "lbrs"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            10584,   // Urok Doomhowl: summoned by the ogre summoner
            10268,   // Gizrul the Slavener: summoned by the Rend Blackhand event
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            10375,  // Spire Spiderling (summoned by Mother Smolderweb, spell 16103)
            9218,   // Spirestone Battle Lord
            10601,  // Urok Enforcer (Urok Doomhowl event summon)
            // Classic casters and healers on map 229 (VMaNGOS spawn and cast data)
            9045,   // Scarshield Acolyte
            9269,   // Smolderthorn Seer
            9239,   // Smolderthorn Mystic
            9262,   // Firebrand Invoker
            9261,   // Firebrand Darkweaver
            9817,   // Blackhand Dreadweaver
            9818,   // Blackhand Summoner
            9240,   // Smolderthorn Shadow Priest
        };
        a.mandatory_interrupt_spells = {
            // Highlord Omokk
            23462,  // Fire Nova
            20691,  // Cleave
            16805,  // Conflagration
            15548,  // Thunderclap
            // Mother Smolderweb
            16104,  // Crystallize
            16468,  // Mother's Milk
            16103,  // Summon Spire Spiderling
            // Gizrul
            16128,  // Infected Bite
            // Quartermaster Zigris
            10101,  // Knock Away
            // Pyroguard Emberseer
            17274,  // Pyroblast
            23341,  // Flame Buffet
            15580,  // Strike
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            12039,   // Heal
            12492,   // Healing Wave
            15799,   // Chain Heal
            12466,   // Fireball
            12468,   // Flamestrike
            15242,   // Fireball
            12739,   // Shadow Bolt
            12471,   // Shadow Bolt
            14887,   // Shadow Bolt Volley
            15800,   // Mana Burn
            15785,   // Mana Burn
            15982,   // Healing Wave
            15659,   // Chain Lightning
            16006,   // Chain Lightning
            15305,   // Chain Lightning
        };
        a.cc_priority_entries = {
            9817,   // Blackhand Dreadweaver
            9818,   // Blackhand Summoner
            9240,   // Smolderthorn Shadow Priest
            9098,   // Scarshield Spellbinder
            9261,   // Firebrand Darkweaver
            9262,   // Firebrand Invoker
            9257,   // Scarshield Warlock
            9045,   // Scarshield Acolyte
            10374,   // Spire Spider
        };
        a.dangerous_auras = {
            // Halycon / dragon adds
            16167,  // Rend Mounts
            16359,  // Corrosive Acid
            16390,  // Flame Breath
            // Smolderweb / general
            8269,   // Frenzy
            16495,  // Fatal Bite
            13738,  // Rend
            3391,   // Thrash
            // Emberseer
            13376,  // Fire Shield
            23462,  // Fire Nova
        };
        // Boss progression — entries from TC's blackrock_spire.h.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            9196,   // Highlord Omokk
            9236,   // Shadow Hunter Vosh'gajin
            9237,   // War Master Voone
            10596,  // Mother Smolderweb
            9736,   // Quartermaster Zigris
            10220,  // Halycon
            9568,   // Overlord Wyrmthalak (final)
            9816,   // Pyroguard Emberseer (intro / event)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 229), in encounter order. Urok Doomhowl and Gizrul are
        // event/script summons with no creature rows and are skipped;
        // the pathfinder routes the corridors between waypoints.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 229), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { -22.7f, -299.8f, 31.7f },   // Highlord Omokk
            { -121.2f, -482.2f, 24.7f },   // Shadow Hunter Vosh'gajin
            { -17.0f, -459.1f, -18.6f },   // War Master Voone
            { -135.5f, -565.9f, 10.2f },   // Mother Smolderweb
            { -199.1f, -458.9f, 87.4f },   // Quartermaster Zigris
            { -193.9f, -338.1f, 64.5f },   // Halycon
            { -22.6f, -486.2f, 90.8f },   // Overlord Wyrmthalak
            { 144.4f, -258.0f, 96.4f },   // Pyroguard Emberseer
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeLowerBlackrockSpireScript()
{
    return std::make_unique<LowerBlackrockSpireScript>();
}

} // namespace Playerbot
