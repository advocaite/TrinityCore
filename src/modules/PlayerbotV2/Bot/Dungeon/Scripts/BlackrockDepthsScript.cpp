// BlackrockDepthsScript — Blackrock Depths (map 230, vanilla 50-60).
// Massive multi-wing dungeon. ~17 bosses including arena event,
// Emperor Thaurissan, Princess Moira, Lord Roccor, Bael'Gar, etc.
//
// Authoritative spell IDs from TC source:
//   src/server/scripts/EasternKingdoms/BlackrockMountain/BlackrockDepths/boss_*.cpp

#include "../DungeonScript.h"

namespace Playerbot {

namespace {

class BlackrockDepthsScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 230; }
    char const* name() const override { return "blackrock_depths"; }
    // Zero static spawns in the Classic creature data: raised by an event the bots never trigger, so a full clear does not wait for it.
    std::vector<uint32_t> event_summoned_bosses() const override
    {
        return {
            9537,   // Hurley Blackbreath: Ribbly Screwspigot event
        };
    }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            8894,   // Anvilrage Medic
            8904,   // Shadowforge Senator
            8913,   // Twilight Emissary
            8915,   // Twilight's Hammer Ambassador
            8900,   // Doomforge Arcanasmith
            8910,   // Blazing Fireguard
            8911,   // Fireguard Destroyer
            8895,   // Anvilrage Officer
            8898,   // Anvilrage Marshal
        };
        a.mandatory_interrupt_spells = {
            // High Interrogator Gerstahn
            10894,  // Shadow Word: Pain
            10876,  // Mana Burn
            8122,   // Psychic Scream
            22417,  // Shadow Shield
            // Plugger Spazzring (Black-Iron event)
            15573,  // Fireblast
            // Emperor Thaurissan
            17492,  // Hand of Thaurissan
            // Phalanx
            14099,  // Mighty Blow
            9080,   // Hamstring
            20691,  // Cleave
            // Houndmaster Grebmar dogs
            13900,  // Fiery Burst
            // Imperial Priests
            10917,  // Heal
            10929,  // Renew
            10901,  // Power Word: Shield
            10947,  // Mind Blast
            10934,  // Smite
            // Warlock bosses
            15245,  // Shadow Bolt Volley
            12742,  // Immolate
            12493,  // Curse of Weakness
            13787,  // Demon Armor
            15092,  // Summon Voidwalkers
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            15585,   // Prayer of Healing
            15586,   // Heal
            11642,   // Heal
            15587,   // Mind Blast
            12471,   // Shadow Bolt
            14034,   // Fireball
            12466,   // Fireball
            15043,   // Frostbolt
            13745,   // Arcane Explosion
            13952,   // Holy Light
            15493,   // Holy Light
        };
        a.cc_priority_entries = {
            8894,   // Anvilrage Medic
            8904,   // Shadowforge Senator
            8900,   // Doomforge Arcanasmith
            8913,   // Twilight Emissary
            8915,   // Twilight's Hammer Ambassador
            8902,   // Shadowforge Citizen
        };
        a.dangerous_auras = {
            // Bael'Gar
            15636,  // Avatar of Flame
            // Phalanx
            8269,   // Frenzy (general boss enrage)
            // Houndmaster's hounds
            24375,  // War Stomp
            // Plugger
            47310,  // Direbrew Disarm (Coren Direbrew event)
            47442,  // Barreled
            15529,  // Gout of Flame
        };
        // Boss progression — BRD has 16+ encounters in a sprawling layout.
        // Tank-advance picks closest alive; multi-hour clears need
        // progression_waypoints to truly automate.
        // The boss route is index for index with the waypoints below: the owed boss is steered to by its waypoint. A boss with no
        // static spawn (event_summoned_bosses) has no waypoint and is not on it; with one in, every later waypoint answered the
        // wrong boss.
        a.bosses = {
            9018,   // High Interrogator Gerstahn
            9025,   // Lord Roccor
            9319,   // Houndmaster Grebmar
            // Ring of Law: random gladiator (9027-9032, script-spawned by arena event)
            9024,   // Pyromancer Loregrain
            9017,   // Lord Incendius
            9041,   // Warder Stilgiss
            9056,   // Fineous Darkvire
            9016,   // Bael'Gar
            9033,   // General Angerforge
            8983,   // Golem Lord Argelmach
            9502,   // Phalanx
            9543,   // Ribbly Screwspigot
            9499,   // Plugger Spazzring
            9156,   // Ambassador Flamelash
            9039,   // Doom'rel (leader of The Seven / Tomb of the Seven event)
            9938,   // Magmus
            9019,   // Emperor Dagran Thaurissan (final)
            8929,   // Princess Moira Bronzebeard (faction-based)
        };
        // Progression waypoints — boss spawn positions from world.creature
        // (map 230), in encounter order. Ring of Law gladiators are
        // arena-spawned and Princess Moira (8929) has no creature row;
        // both are omitted.
        // Boss spawn positions from the Classic creature data (VMaNGOS spawn table, map 230), in encounter order; bosses a script
        // summons (no static spawn) are left out. Replaces hand-typed coordinates that did not match the map.
        a.progression_waypoints = {
            { 310.6f, -146.3f, -70.3f },   // High Interrogator Gerstahn
            { 615.5f, -267.4f, -83.6f },   // Lord Roccor
            { 607.7f, -174.8f, -84.5f },   // Houndmaster Grebmar
            { 530.2f, -243.9f, -43.0f },   // Pyromancer Loregrain
            { 893.5f, -267.1f, -71.9f },   // Lord Incendius
            { 823.4f, -342.3f, -50.1f },   // Warder Stilgiss
            { 963.3f, -343.7f, -71.7f },   // Fineous Darkvire
            { 702.4f, 184.5f, -72.0f },   // Bael'Gar
            { 652.4f, 21.4f, -60.0f },   // General Angerforge
            { 846.8f, 16.3f, -53.6f },   // Golem Lord Argelmach
            { 847.8f, -230.1f, -43.6f },   // Phalanx
            { 867.1f, -174.0f, -49.8f },   // Ribbly Screwspigot
            { 888.5f, -177.9f, -43.0f },   // Plugger Spazzring
            { 1009.8f, -239.0f, -61.3f },   // Ambassador Flamelash
            { 1281.1f, -282.2f, -78.1f },   // Doom'rel
            { 1380.7f, -659.3f, -92.0f },   // Magmus
            { 1380.2f, -831.6f, -87.6f },   // Emperor Dagran Thaurissan
            { 1384.6f, -831.5f, -87.6f },   // Princess Moira Bronzebeard
        };
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeBlackrockDepthsScript()
{
    return std::make_unique<BlackrockDepthsScript>();
}

} // namespace Playerbot
