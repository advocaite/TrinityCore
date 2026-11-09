// DireMaulScript — Dire Maul (map 429, vanilla 55-60+).
// Three wings: East (DM:E), West (DM:W), North (DM:N).
//   * DM:E — Pusillin chase (rogue, runs). Lethtendris's Imp Mojo
//     party-buff side-mechanic.
//   * DM:W — Tendris Warpwood AoE summon Whirlwind. Magister Doane
//     stacking armor; Illyanna Ravenoak Hunter's Mark.
//   * DM:N — Tribute run (multiple guard groups left alive grants
//     better loot — bots burn everything; suboptimal but completable).
//     King Gordok charges; Cho'Rush dispels boss buffs.

#include "../DungeonScript.h"
#include "../../BotSnapshotView.h"
#include <array>
#include <limits>

namespace Playerbot {

namespace {

class DireMaulScript final : public DungeonScript
{
public:
    uint32_t  map_id() const override { return 429; }
    char const* name() const override { return "dire_maul"; }

    DungeonAdvice get_advice(BotSnapshotView const& s) const override
    {
        DungeonAdvice a;
        a.high_priority_kill_entries = {
            11448,   // Gordok Warlock
            11444,   // Gordok Mage-Lord
            11470,   // Eldreth Sorcerer
            11466,   // Highborne Summoner
            11457,   // Wildspawn Hellcaller
            11455,   // Wildspawn Felsworn
            14398,   // Eldreth Darter
        };
        a.mandatory_interrupt_spells = {
            22417,  // Mortal Strike (King Gordok / DM:N tribunes)
            22336,  // Whirlwind (Magister Doane)
            22456,  // Hunter's Mark (Illyanna Ravenoak)
            22640,  // Polymorph (DM:W casters mind-control)
            // Classic spell ids from the VMaNGOS spell_template / converted mob casts
            13323,   // Polymorph
            15530,   // Frostbolt
            16102,   // Flamestrike
            16006,   // Chain Lightning
            12466,   // Fireball
            15232,   // Shadow Bolt
            15228,   // Fireball
            15537,   // Shadow Bolt
            12542,   // Fear
            22947,   // Mana Burn
            12096,   // Fear
            22414,   // Lightning Bolt
        };
        a.cc_priority_entries = {
            11448,   // Gordok Warlock
            11444,   // Gordok Mage-Lord
            11470,   // Eldreth Sorcerer
            11466,   // Highborne Summoner
            11457,   // Wildspawn Hellcaller
            11455,   // Wildspawn Felsworn
        };
        a.dangerous_auras = {
            22425,  // Charge (King Gordok knockback)
        };
        // The boss route is the wing the bot stands in, not the whole map. One list for the three wings sent every party to the East
        // wing's first boss: b3ca0b4b67 dmnorth_moldar, Pusillin was alive 250 y away behind the wing walls (reach 0, "no progress
        // toward the target"), the tank inched over the North wing's first yards for a minute and the group stood behind it. The
        // wings share map 429 but do not join inside; the nearest wing entrance (the dmeast/dmwest/dmnorth scenario starts) or boss
        // spawn names the wing, the same as the Scarlet Monastery script. Boss spawns from the Classic creature data (VMaNGOS spawn
        // table, map 429), in encounter order; a boss a script summons (no static spawn) is left out.
        struct Spot { float x, y, z; };
        struct Wing { std::vector<uint32_t> bosses; std::vector<Spot> spots; Spot entrance; };
        static std::array<Wing, 3> const wings = {{
            { { 14354,    // Pusillin
                13280,    // Hydrospawn
                14327,    // Lethtendris
                11490,    // Zevrim Thornhoof
                11492 },  // Alzzin the Wildshaper (East final)
              { { 83.4f, -198.4f, -4.0f }, { -16.6f, -427.6f, -59.8f }, { -7.8f, -443.8f, 16.4f }, { -35.0f, -448.0f, -37.9f },
                { 260.9f, -436.8f, -120.0f } },
              { -203.166f, -322.997f, -2.72467f } },
            { { 11489,    // Tendris Warpwood
                11487,    // Magister Kalendris
                11488,    // Illyanna Ravenoak
                11467,    // Tsu'zee
                11496,    // Immol'thar
                11486 },  // Prince Tortheldrin (West final)
              { { 13.5f, 478.3f, -23.3f }, { -106.3f, 551.3f, -4.4f }, { -14.6f, 542.1f, 28.6f }, { 128.6f, 561.8f, -4.3f },
                { -40.5f, 811.1f, -29.4f }, { 132.6f, 625.9f, -48.4f } },
              { 33.1083f, 158.977f, -3.47126f } },
            { { 14326,    // Guard Mol'dar
                14322,    // Stomper Kreeg
                14321,    // Guard Fengus
                14323,    // Guard Slip'kik
                14325,    // Captain Kromcrush
                14324,    // Cho'Rush the Observer
                11501 },  // King Gordok (North final)
              { { 396.5f, -8.6f, -24.9f }, { 494.2f, 99.7f, -2.6f }, { 384.7f, 341.3f, 2.9f }, { 559.2f, 549.4f, -25.4f },
                { 627.6f, 481.7f, 29.5f }, { 835.4f, 487.9f, 37.3f }, { 829.7f, 481.3f, 37.4f } },
              { 254.92f, -19.463f, -2.5596f } },
        }};
        auto add = [&a](Wing const& w)
        {
            a.bosses.insert(a.bosses.end(), w.bosses.begin(), w.bosses.end());
            for (Spot const& p : w.spots)
                a.progression_waypoints.push_back({ p.x, p.y, p.z });
        };
        // No bot in the map (the GM audit and list commands pass an empty view): every wing, in the old order.
        if (s.map_id() != map_id())
        {
            for (Wing const& w : wings)
                add(w);
            return a;
        }
        float px, py, pz;
        s.position(px, py, pz);
        Wing const* in = &wings[0];
        float best = std::numeric_limits<float>::max();
        for (Wing const& w : wings)
        {
            auto consider = [&](Spot const& p)
            {
                float const d = (p.x - px) * (p.x - px) + (p.y - py) * (p.y - py);
                if (d < best) { best = d; in = &w; }
            };
            consider(w.entrance);
            for (Spot const& p : w.spots)
                consider(p);
        }
        add(*in);
        return a;
    }
};

} // anonymous

std::unique_ptr<DungeonScript> MakeDireMaulScript()
{
    return std::make_unique<DireMaulScript>();
}

} // namespace Playerbot
