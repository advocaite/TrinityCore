// RagefireChasmScript — Ragefire Chasm (map 389, modern post-Cata
// remake L13-21). Tiny Horde starter dungeon under Orgrimmar. TC's
// instance script defines four encounters in canonical order:
//   1) Adarogg                  (61408)
//   2) Dark Shaman Koranthal    (61412)
//   3) Slagmaw                  (61463)
//   4) Lava Guard Gordoth       (61528) — final
// The layout is a clockwise spiral down/around a central lava pool.
// Tank-advance walks the progression_waypoints[] in order; each point
// is meant to land the tank inside the next mob pack's aggro radius.
// Detour validates each move so an out-of-date waypoint just gets
// rejected — bot stays put until trash/boss is in nearby_enemies.

#include "../DungeonScript.h"
#include "../../ClassTables.h"

namespace Playerbot {

namespace {

// Classic 1.60 (Forever): the vanilla dungeon. The script below carries the retail remake's ids, which do not exist there.
DungeonAdvice RagefireChasmClassicAdvice()
{
    DungeonAdvice a;
    // Creature ids and spell ids from the VMaNGOS data Forever's Classic world was built from (creature_template, creature spawns,
    // spell_template); the casts come from the converted EventAI / spell lists in 2026_09_28_15_world_vanilla_smartai.sql.
    a.high_priority_kill_entries = {
        11324,   // Searing Blade Warlock
        11319,   // Ragefire Shaman
        11322,   // Searing Blade Cultist
    };
    a.mandatory_interrupt_spells = {
        11986,   // Healing Wave
        20791,   // Shadow Bolt
        9532,   // Lightning Bolt
        20800,   // Immolate
    };
    a.cc_priority_entries = {
        11324,   // Searing Blade Warlock
        11319,   // Ragefire Shaman
    };
    a.bosses = {
        11517,   // Oggleflint
        11520,   // Taragaman the Hungerer
        11518,   // Jergosh the Invoker
        11519,   // Bazzalan
    };
    a.progression_waypoints = {
        { -147.5f, 38.7f, -38.8f },   // Oggleflint
        { -244.7f, 150.1f, -18.7f },   // Taragaman the Hungerer
        { -376.8f, 209.2f, -21.8f },   // Jergosh the Invoker
        { -384.9f, 146.0f, 7.8f },   // Bazzalan
    };
    // The crumb route through the four vanilla bosses in order. The shared route table's map 389 chain is the retail remake's
    // (Adarogg's pit, then up to Koranthal's walkway): it never goes down to Oggleflint, its crumb nearest him is the walkway 18 yards
    // above his floor, and the tank followed it into the pit's dead end and back up onto that ledge, where it stood for the rest of
    // the run (0.1.24 rfc_oggleflint). Oggleflint's floor is reached from the pit through the low tunnel east (-233,-8 -> -192,0 ->
    // -151,6). Generated from the server's map 389 navmesh: the full Detour route entrance -> Oggleflint -> Taragaman -> Jergosh ->
    // Bazzalan, sampled every 20 yards (mmap_probe <mmaps> 389 --crumbs 20 <boss> <next boss>, one leg per boss); each crumb pair is a
    // complete path within the 74-polygon cap, and each boss's nearest crumb is the one on its spawn.
    a.route_waypoints = {
        { 0.8f, -8.2f, -15.3f }, { -4.6f, -26.6f, -20.8f }, { -15.7f, -40.2f, -21.3f }, { -35.6f, -39.7f, -21.5f },
        { -55.4f, -38.8f, -20.1f }, { -75.2f, -37.8f, -18.5f }, { -93.4f, -37.0f, -26.3f }, { -112.6f, -36.1f, -30.8f },
        { -132.4f, -35.2f, -32.8f }, { -152.1f, -34.3f, -35.5f }, { -170.8f, -33.4f, -41.7f }, { -190.2f, -32.5f, -43.8f },
        { -209.6f, -31.6f, -51.1f }, { -228.1f, -30.8f, -55.6f }, { -247.8f, -29.8f, -58.1f }, { -259.9f, -14.9f, -57.6f },
        { -251.3f, -8.9f, -62.5f }, { -231.4f, -8.2f, -61.6f }, { -211.8f, -4.1f, -61.5f }, { -192.4f, -0.1f, -59.6f },
        { -174.1f, 2.4f, -52.0f }, { -156.6f, 4.8f, -42.5f }, { -149.4f, 18.8f, -38.3f },
        { -147.5f, 38.7f, -38.6f },   // 23 Oggleflint
        { -149.4f, 18.8f, -38.3f }, { -156.6f, 4.8f, -42.5f }, { -174.1f, 2.4f, -52.0f }, { -192.4f, -0.1f, -59.6f },
        { -211.8f, -4.1f, -61.5f }, { -231.4f, -8.2f, -61.6f }, { -251.2f, -8.9f, -62.5f }, { -270.7f, -11.9f, -63.0f },
        { -285.0f, -24.0f, -59.6f }, { -295.7f, -35.6f, -60.5f }, { -299.7f, -16.9f, -55.9f }, { -297.6f, 0.5f, -47.7f },
        { -278.3f, 4.8f, -48.3f }, { -258.9f, 8.4f, -49.5f }, { -239.7f, 12.1f, -44.8f }, { -223.7f, 19.4f, -41.1f },
        { -204.6f, 21.3f, -36.5f }, { -185.9f, 19.4f, -31.4f }, { -166.7f, 17.3f, -28.2f }, { -147.9f, 15.2f, -21.8f },
        { -128.2f, 16.2f, -19.5f }, { -118.5f, 31.8f, -17.4f }, { -120.2f, 51.3f, -18.7f }, { -135.2f, 64.0f, -22.1f },
        { -152.3f, 72.8f, -21.1f }, { -171.1f, 79.6f, -20.7f }, { -186.6f, 90.6f, -23.3f }, { -191.4f, 109.7f, -23.4f },
        { -194.6f, 128.7f, -22.1f }, { -197.6f, 147.3f, -24.6f }, { -215.8f, 151.5f, -20.2f }, { -235.7f, 150.5f, -18.7f },
        { -244.7f, 150.1f, -18.7f },   // 56 Taragaman the Hungerer
        { -264.6f, 151.5f, -18.8f }, { -283.5f, 152.8f, -25.4f }, { -293.6f, 168.9f, -21.0f }, { -298.5f, 187.8f, -23.9f },
        { -306.3f, 206.1f, -24.6f }, { -320.9f, 214.5f, -20.4f }, { -340.9f, 213.4f, -21.0f }, { -360.7f, 211.1f, -21.7f },
        { -376.8f, 209.2f, -21.5f },   // 65 Jergosh the Invoker
        { -364.6f, 224.8f, -20.0f }, { -350.8f, 233.1f, -19.8f }, { -332.8f, 235.9f, -15.4f }, { -336.2f, 248.4f, -7.9f },
        { -354.7f, 247.2f, -6.9f }, { -372.2f, 238.5f, -4.1f }, { -384.6f, 226.8f, -0.3f }, { -398.5f, 212.8f, -0.4f },
        { -402.1f, 195.6f, 4.4f }, { -398.3f, 177.9f, 11.3f }, { -391.3f, 161.3f, 8.3f },
        { -384.9f, 146.0f, 8.0f },   // 77 Bazzalan
    };
    return a;
}

class RagefireChasmScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 389; }
    char const* name() const override { return "ragefire_chasm"; }

    DungeonAdvice get_advice(BotSnapshotView const& /*s*/) const override
    {
        if (IsVanillaRealm())
            return RagefireChasmClassicAdvice();
        DungeonAdvice a;
        // Boss order MUST match the DB crumb-route order (wowc_playerbot.
        // playerbot_dungeon_routes map389, 54 crumbs): the route descends into
        // Adarogg's pit FIRST (crumbs 1-13, entrance→-285,-54,-61) then rises to
        // Koranthal (24) → Slagmaw (32) → Gordoth (53). That IS the TC encounter
        // index order. bosses_done_count indexes both bosses[] and the escape's
        // progression_waypoints[], and the tank-advance pushes the earliest LIVE
        // bosses[i] — so a Koranthal-first reorder (tried 07-22) fights the route:
        // the tank beelined 104y NW toward Koranthal OFF the route and stalled,
        // because the route cursor still wanted the Adarogg descent first. Keep
        // encounter/route order.
        a.bosses = {
            61408,  // Adarogg               (SW pit, z-60 — route crumb 13)
            61412,  // Dark Shaman Koranthal (NE, z-20   — route crumb 24)
            61463,  // Slagmaw               (N,  z-19   — route crumb 32)
            61528,  // Lava Guard Gordoth    (NW, z-18   — route crumb 53)
        };
        // Progression waypoints — the ACTUAL boss spawn positions, aligned
        // 1:1 with bosses[] above (waypoints[i] == bosses[i]). This alignment
        // is REQUIRED: the false-combat escape (State_Idle) relocates a wedged
        // bot to waypoints[bosses_done_count] and its logic asserts
        // waypoints[i]==bosses[i] "is exactly the encounter we still owe".
        //
        // ROOT-FIX 2026-07-22: the previous coords were mis-surveyed by
        // 200-400y (e.g. Koranthal waypoint (38.7,-90) vs the real (-117,71))
        // AND off-by-one (a leading "ramp" entry shifted every boss). The tank-
        // advance's Detour check therefore rejected every waypoint as
        // unreachable geometry, so the group never routed to a pack — it drifted
        // via incidental aggro, stalling 60-80y short of each boss while a bot
        // sat in false-combat, and the escape teleported to the wrong point.
        // Coords are the live wc_world.creature spawn positions on map 389
        // (verified reachable — the squad kills these bosses when it arrives).
        a.progression_waypoints = {
            { -284.6f,  -53.8f, -60.7f },   // 61408 Adarogg (pit — route crumb 13)
            { -117.0f,   71.1f, -20.7f },   // 61412 Dark Shaman Koranthal
            { -257.2f,  172.4f, -19.6f },   // 61463 Slagmaw
            { -369.5f,  166.2f, -18.5f },   // 61528 Lava Guard Gordoth
        };
        // Boss-specific interrupts / dangerous casts. Spell IDs from TC
        // boss scripts (or WoWHead) — populate as we identify them
        // empirically. Empty list = no override, generic interrupt rule
        // still kicks any interruptible cast.
        a.mandatory_interrupt_spells = {};
        a.dangerous_auras = {};
        a.cc_priority_entries = {};
        a.high_priority_kill_entries = {};
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeRagefireChasmScript()
{
    return std::make_unique<RagefireChasmScript>();
}

} // namespace Playerbot
