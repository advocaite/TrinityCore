// ZonePicker — distribution destinations for population spread.
//
// Modern WoW (post-squish, no Chromie Time on this server) uses
// per-zone DYNAMIC LEVEL SCALING: a zone has a bracket (e.g. Hellfire
// Peninsula 10-30) and mobs/quests scale to the bot's actual level
// within that bracket. So a L25 bot entering Hellfire gets L25 mobs;
// a L15 bot in the same zone gets L15 mobs. The bracket gates ENTRY
// (some entry quests have minimum-level prereqs), not the leveling
// content itself.
//
// Goal of this picker: SPREAD bots across the entire game world so
// every expansion's zones get populated, not just the latest one.
// "Linear-progression-only" routing was wrong — it sent every L40 bot
// to vanilla EPL and left Outland / Northrend / Pandaria / Draenor /
// Legion / BfA / Shadowlands empty.
//
// Each (level-bracket, faction) entry advertises ONE candidate zone
// hub. The picker walks the table, collects all rows that match the
// bot's level-bracket + faction, and round-robins via the bot's GUID
// seed. Add as many candidates as you want — more candidates = wider
// spread.
//
// Some expansion entry-quests have minimum-level prereqs (e.g. WoD
// Tanaan 90+, Legion Broken Isles 98+ retail-historical). The bracket
// `lo` field encodes that floor; the picker won't route a L20 bot to
// Khaz Algar even if other candidates exist. Brackets overlap heavily:
// L40 has Outland + Northrend + Pandaria + WoD + Legion + BfA + SL +
// vanilla Tanaris + others all valid, so 200 L40 bots split ~25 ways.

#include "ZonePicker.h"

#include "Bot/ClassTables.h"   // IsVanillaRealm
#include "DB2Stores.h"
#include "Log.h"
#include "World.h"
#include "Map.h"
#include "MapManager.h"
#include "PhasingHandler.h"
#include "SpreadSpot.h"
#include "GrindSpot.h"
#include "ObjectMgr.h"
#include "CreatureData.h"
#include "../../Services.h"
#include "../../Travel/QuestHubDatabase.h"

#include <array>
#include <cstring>
#include <vector>

namespace Playerbot::V2::World {

namespace {

struct BracketEntry
{
    uint8  lo, hi;             // inclusive level bracket
    bool   alliance;           // true = Alliance only, false = Horde only
    uint32 map_id;
    float  x, y, z;
    char const* zone;
    uint32 starter_quest;
};

// Comprehensive candidate list. Brackets overlap to spread bots across
// every expansion's zone instead of clustering them by level.
constexpr std::array<BracketEntry, 88> kBrackets = {{
    // ===== L0-9 catch-all (faction capital) =====
    {  0,  9, true,  0, -8833.0f,   628.0f,   94.0f, "Stormwind",                            0 },
    {  0,  9, false, 1,  1633.0f, -4439.0f,   16.0f, "Orgrimmar",                            0 },

    // ===== Vanilla zones — Eastern Kingdoms / Kalimdor (L10-60) =====
    // Alliance starter chains
    { 10, 60, true,  0, -10752.0f,  1252.0f,   42.0f, "Westfall (Sentinel Hill)",            0 },
    { 10, 60, true,  0,  -5455.0f, -3413.0f,  306.0f, "Loch Modan (Thelsamar)",              0 },
    { 10, 60, true,  1,   6256.0f,    16.0f,    9.0f, "Darkshore (Auberdine)",               0 },
    { 15, 60, true,  0,  -3796.0f,  -777.0f,   11.0f, "Wetlands (Menethil Harbor)",          0 },
    { 15, 60, true,  0,  -2110.0f, -2480.0f,   99.0f, "Hillsbrad Foothills (Southshore)",    0 },
    { 20, 60, true,  0, -14470.0f,   471.0f,   23.0f, "Northern Stranglethorn (Rebel Camp)", 0 },
    { 25, 60, true,  1,  -7173.0f, -3791.0f,    8.0f, "Tanaris (Gadgetzan)",                 0 },
    { 25, 60, true,  1,  -4413.0f,   324.0f,   47.0f, "Feralas (Feathermoon Stronghold)",    0 },
    { 30, 60, true,  0,  -6831.0f, -2030.0f,  244.0f, "Searing Gorge (Thorium Point)",       0 },
    { 35, 60, true,  0,   2283.0f, -5318.0f,   89.0f, "Eastern Plaguelands (Light's Hope)",  0 },
    { 40, 60, true,  1,   7479.0f, -4738.0f,  687.0f, "Winterspring (Everlook)",             0 },
    { 40, 60, true,  1,  -7045.0f,  1242.0f,    7.0f, "Silithus (Cenarion Hold)",            0 },
    // Horde starter chains
    { 10, 60, false, 0,   1822.0f,  1599.0f,   90.0f, "Tirisfal Glades (Brill)",             0 },
    { 10, 60, false, 0,    367.0f,  1452.0f,  124.0f, "Silverpine Forest (Sepulcher)",       0 },
    { 10, 60, false, 1,   -444.0f, -2645.0f,   96.0f, "Northern Barrens (Crossroads)",       0 },
    { 15, 60, false, 1,   2266.0f, -2532.0f,  104.0f, "Stonetalon Mountains (Sun Rock Retreat)", 0 },
    { 20, 60, false, 0, -11919.0f, -1209.0f,   92.0f, "Northern Stranglethorn (Grom'gol)",   0 },
    { 25, 60, false, 1,    194.0f,  1819.0f,   85.0f, "Desolace (Shadowprey Village)",       0 },
    { 25, 60, false, 1,  -7173.0f, -3791.0f,    8.0f, "Tanaris (Gadgetzan)",                 0 },
    { 30, 60, false, 0,  -7906.0f, -1135.0f,  133.0f, "Burning Steppes (Flame Crest)",       0 },
    { 35, 60, false, 0,  -1283.0f,   140.0f,   58.0f, "Western Plaguelands (The Bulwark)",   0 },
    { 40, 60, false, 1,   7479.0f, -4738.0f,  687.0f, "Winterspring (Everlook)",             0 },
    { 40, 60, false, 1,  -7045.0f,  1242.0f,    7.0f, "Silithus (Cenarion Hold)",            0 },

    // ===== Outland (TBC) — L10-30 retail-historical, scales =====
    { 10, 60, true,  530,   -319.0f,  1697.0f,   71.0f, "Hellfire Peninsula (Honor Hold)",   9476 },
    { 15, 60, true,  530,  -3025.0f,  3441.0f,   55.0f, "Zangarmarsh (Telredor)",            9991 },
    { 20, 60, true,  530,  -1838.0f,  5301.0f,   46.0f, "Nagrand (Telaar)",                  9978 },
    { 10, 60, false, 530,   -371.0f,  3082.0f,   90.0f, "Hellfire Peninsula (Thrallmar)",    10119 },
    { 15, 60, false, 530,  -1262.0f,  5544.0f,   22.0f, "Zangarmarsh (Zabra'jin)",           10001 },
    { 20, 60, false, 530,  -1374.0f,  7282.0f,   31.0f, "Nagrand (Garadar)",                 9991 },

    // ===== Northrend (WotLK) — L10-30 retail-historical, scales =====
    { 10, 60, true,  571,   2730.0f,  2169.0f,    7.0f, "Borean Tundra (Valiance Keep)",     0 },
    { 10, 60, true,  571,   1196.6f, -2890.1f,   39.1f, "Howling Fjord (Valgarde)",          0 },
    { 25, 60, true,  571,   3722.0f,  -706.0f,  213.0f, "Dragonblight (Wintergarde Keep)",   0 },
    { 10, 60, false, 571,   2078.0f,  5258.0f,   18.0f, "Borean Tundra (Warsong Hold)",      0 },
    { 10, 60, false, 571,   1832.0f, -6131.0f,   47.0f, "Howling Fjord (Vengeance Landing)", 0 },
    { 25, 60, false, 571,   3633.0f,  -758.0f,  211.0f, "Dragonblight (Agmar's Hammer)",     0 },

    // ===== Cataclysm zones (Hyjal, Vashj'ir, Twilight Highlands, Uldum, Deepholm) =====
    { 30, 60, true,  1,    4665.0f, -1825.0f,  1086.0f, "Mount Hyjal (Nordrassil)",          25372 },
    { 30, 60, true,  0,   -5408.0f, -2434.0f,   23.7f, "Twilight Highlands (Highbank)",      27302 },
    { 30, 60, true,  0,  -10864.0f,  1656.0f,   55.0f, "Uldum (Schnottz's Landing)",         27787 },
    { 30, 60, true,  646,  -724.0f,   846.0f,  207.0f, "Deepholm (Temple of Earth)",         27148 },
    { 30, 60, false, 1,    4665.0f, -1825.0f,  1086.0f, "Mount Hyjal (Nordrassil)",          25372 },
    { 30, 60, false, 0,   -3792.0f, -4655.0f,    9.0f, "Twilight Highlands (Bloodgulch)",    27518 },
    { 30, 60, false, 0,  -10864.0f,  1656.0f,   55.0f, "Uldum (Schnottz's Landing)",         27787 },
    { 30, 60, false, 646,  -724.0f,   846.0f,  207.0f, "Deepholm (Temple of Earth)",         27148 },

    // ===== Pandaria (MoP) =====
    { 30, 60, true,  870,   -700.0f, -4506.0f,   14.0f, "Jade Forest (Paw'don Village)",     31482 },
    { 30, 60, true,  870,   3200.0f,  3019.0f,  120.0f, "Townlong Steppes (Longying Outpost)", 0 },
    { 30, 60, false, 870,   1326.0f, -4566.0f,   21.0f, "Jade Forest (Honeydew Village)",    31781 },
    { 30, 60, false, 870,   3200.0f,  3019.0f,  120.0f, "Townlong Steppes (Longying Outpost)", 0 },

    // ===== Draenor (WoD) =====
    { 40, 60, true,  1116, 5710.0f,   4504.0f,  133.0f, "Frostfire Ridge (Lunarfall)",       34362 },
    { 40, 60, true,  1116, 1804.0f,   2032.0f,  191.0f, "Talador (Vol'mar)",                 0 },
    { 40, 60, false, 1116, 5710.0f,   4504.0f,  133.0f, "Frostfire Ridge (Frostwall)",       34480 },
    { 40, 60, false, 1116, 1804.0f,   2032.0f,  191.0f, "Talador (Vol'jin's Pride)",         0 },

    // ===== Legion (Broken Isles) =====
    { 45, 60, true,  1220, -1791.0f,  1428.0f,  246.0f, "Dalaran (Broken Isles)",            40519 },
    { 45, 60, true,  1220,  1300.0f,  4500.0f,  100.0f, "Stormheim (Greywatch)",             40519 },
    { 45, 60, true,  1220,  4392.0f,  3891.0f,  168.0f, "Highmountain (Thunder Totem)",      40519 },
    { 45, 60, false, 1220, -1791.0f,  1428.0f,  246.0f, "Dalaran (Broken Isles)",            40519 },
    { 45, 60, false, 1220,  1300.0f,  4500.0f,  100.0f, "Stormheim (Greywatch)",             40519 },
    { 45, 60, false, 1220,  4392.0f,  3891.0f,  168.0f, "Highmountain (Thunder Totem)",      40519 },

    // ===== BfA (Kul Tiras / Zandalar) =====
    { 50, 60, true,  1643,  -1058.0f, -2691.0f,    8.0f, "Tiragarde Sound (Boralus)",        51571 },
    { 50, 60, true,  1643,  -2030.0f, -1650.0f,   40.0f, "Drustvar (Falconhurst)",           51571 },
    { 50, 60, false, 1642,  -1126.0f,  3158.0f,   29.0f, "Zuldazar (Dazar'alor)",            52133 },
    { 50, 60, false, 1642,   2456.0f,   824.0f,   80.0f, "Vol'dun (Vorrik's Sanctum)",       52133 },

    // ===== Shadowlands (Bastion / Maldraxxus / Ardenweald / Revendreth) =====
    { 50, 60, true,  1550,  -1646.0f,  -641.0f,  280.0f, "Bastion (Aspirant's Rest)",        62130 },
    { 50, 60, true,  1550,    1190.0f,  -772.0f,  -27.0f, "Maldraxxus (Theater of Pain)",    62130 },
    { 50, 60, true,  1550,    3142.0f,  3997.0f, 1010.0f, "Ardenweald (Heart of the Forest)", 62130 },
    { 50, 60, true,  1550,    1820.0f,   805.0f,    8.0f, "Revendreth (Sinfall)",            62130 },
    { 50, 60, false, 1550,  -1646.0f,  -641.0f,  280.0f, "Bastion (Aspirant's Rest)",        62130 },
    { 50, 60, false, 1550,    1190.0f,  -772.0f,  -27.0f, "Maldraxxus (Theater of Pain)",    62130 },
    { 50, 60, false, 1550,    3142.0f,  3997.0f, 1010.0f, "Ardenweald (Heart of the Forest)", 62130 },
    { 50, 60, false, 1550,    1820.0f,   805.0f,    8.0f, "Revendreth (Sinfall)",            62130 },

    // ===== Dragonflight (Dragon Isles) =====
    { 60, 70, true,  2444,  -1850.0f,  5448.0f,   34.0f, "Forbidden Reach (DF intro)",       65437 },
    { 60, 70, true,  2444,   3550.0f,  3950.0f,   55.0f, "Waking Shores (Wingrest)",         65437 },
    { 60, 70, true,  2444,   2600.0f,   100.0f,  300.0f, "Thaldraszus (Valdrakken)",         65437 },
    { 60, 70, false, 2444,  -1850.0f,  5448.0f,   34.0f, "Forbidden Reach (DF intro)",       66775 },
    { 60, 70, false, 2444,   3550.0f,  3950.0f,   55.0f, "Waking Shores (Wingrest)",         66775 },
    { 60, 70, false, 2444,   2600.0f,   100.0f,  300.0f, "Thaldraszus (Valdrakken)",         66775 },

    // ===== TWW (Khaz Algar) =====
    { 70, 80, true,  2552,   -100.0f, -2700.0f,  105.0f, "Isle of Dorn (Dornogal)",          78714 },
    { 70, 80, true,  2552,    1800.0f,  -150.0f,   50.0f, "Hallowfall (Mereldar)",           78714 },
    { 70, 80, false, 2552,   -100.0f, -2700.0f,  105.0f, "Isle of Dorn (Dornogal)",          78714 },
    { 70, 80, false, 2552,    1800.0f,  -150.0f,   50.0f, "Hallowfall (Mereldar)",           78714 },

    // ===== Midnight (Quel'Thalas) =====
    { 80, 90, true,  0,    9487.0f, -7277.0f,   14.0f, "Silvermoon City (Quel'Thalas)",     0 },
    { 80, 90, true,  0,    7600.0f, -6200.0f,   38.0f, "Eversong Woods (Falconwing Square)", 0 },
    { 80, 90, false, 0,    9487.0f, -7277.0f,   14.0f, "Silvermoon City (Quel'Thalas)",     0 },
    { 80, 90, false, 0,    7600.0f, -6200.0f,   38.0f, "Eversong Woods (Falconwing Square)", 0 },
}};

// ===== Classic 1.60 (Forever): vanilla towns =====
//
// The table above is retail (scaled zones, Outland / Northrend / Pandaria /
// Dragon Isles / Khaz Algar maps, expansion gate quests). On a vanilla realm
// a world bot belongs in the 1.12 zone that fits its level, on Eastern
// Kingdoms (0) or Kalimdor (1). Each row is a vanilla town's flight master
// with the zone's 1.12 level range; the position is NOT hand-typed: it is
// resolved from TaxiNodes.db2 (the node whose name starts with the town, on
// that continent, shown on the bot faction's map), so the bot lands on the
// flight master's real ground point. A row whose node the data lacks is
// dropped (logged once) instead of guessed.
struct VanillaTown
{
    char const* town;    // TaxiNodes name up to the comma ("Sentinel Hill, Westfall")
    uint16 map;
    uint8  lo, hi;       // 1.12 zone level range
    bool   alliance, horde;
};

constexpr VanillaTown kVanillaTowns[] = {
    // Alliance
    { "Sentinel Hill",       0, 10, 20, true,  false },   // Westfall
    { "Thelsamar",           0, 10, 20, true,  false },   // Loch Modan
    { "Auberdine",           1, 11, 20, true,  false },   // Darkshore
    { "Lakeshire",           0, 15, 25, true,  false },   // Redridge Mountains
    { "Stonetalon Peak",     1, 15, 27, true,  false },   // Stonetalon Mountains
    { "Astranaar",           1, 18, 30, true,  false },   // Ashenvale
    { "Darkshire",           0, 18, 30, true,  false },   // Duskwood
    { "Menethil Harbor",     0, 20, 30, true,  false },   // Wetlands
    { "Southshore",          0, 20, 30, true,  false },   // Hillsbrad Foothills
    { "Refuge Pointe",       0, 30, 40, true,  false },   // Arathi Highlands
    { "Nijel's Point",       1, 30, 40, true,  false },   // Desolace
    { "Theramore",           1, 35, 45, true,  false },   // Dustwallow Marsh
    { "Thalanaar",           1, 40, 50, true,  false },   // Feralas
    { "Feathermoon",         1, 40, 50, true,  false },   // Feralas
    { "Aerie Peak",          0, 40, 50, true,  false },   // The Hinterlands
    { "Nethergarde Keep",    0, 45, 55, true,  false },   // Blasted Lands
    { "Talrendis Point",     1, 45, 55, true,  false },   // Azshara
    { "Talonbranch Glade",   1, 48, 55, true,  false },   // Felwood
    { "Morgan's Vigil",      0, 50, 60, true,  false },   // Burning Steppes
    { "Chillwind Camp",      0, 51, 58, true,  false },   // Western Plaguelands
    // Horde
    { "The Sepulcher",       0, 10, 20, false, true  },   // Silverpine Forest
    { "Crossroads",          1, 10, 25, false, true  },   // The Barrens
    { "Camp Taurajo",        1, 15, 25, false, true  },   // The Barrens
    { "Sun Rock Retreat",    1, 15, 27, false, true  },   // Stonetalon Mountains
    { "Splintertree Post",   1, 18, 30, false, true  },   // Ashenvale
    { "Zoram'gar Outpost",   1, 20, 30, false, true  },   // Ashenvale
    { "Tarren Mill",         0, 20, 30, false, true  },   // Hillsbrad Foothills
    { "Freewind Post",       1, 25, 35, false, true  },   // Thousand Needles
    { "Hammerfall",          0, 30, 40, false, true  },   // Arathi Highlands
    { "Shadowprey Village",  1, 30, 40, false, true  },   // Desolace
    { "Grom'gol",            0, 30, 45, false, true  },   // Stranglethorn Vale
    { "Kargath",             0, 35, 45, false, true  },   // Badlands
    { "Stonard",             0, 35, 45, false, true  },   // Swamp of Sorrows
    { "Brackenwall Village", 1, 35, 45, false, true  },   // Dustwallow Marsh
    { "Camp Mojache",        1, 40, 50, false, true  },   // Feralas
    { "Revantusk Village",   0, 40, 50, false, true  },   // The Hinterlands
    { "Valormok",            1, 45, 55, false, true  },   // Azshara
    { "Bloodvenom Post",     1, 48, 55, false, true  },   // Felwood
    { "Flame Crest",         0, 50, 60, false, true  },   // Burning Steppes
    // Both factions (each side resolves its own node)
    { "Ratchet",             1, 15, 25, true,  true  },   // The Barrens
    { "Booty Bay",           0, 30, 45, true,  true  },   // Stranglethorn Vale
    { "Gadgetzan",           1, 40, 50, true,  true  },   // Tanaris
    { "Thorium Point",       0, 43, 50, true,  true  },   // Searing Gorge
    { "Marshal's Refuge",    1, 48, 55, true,  true  },   // Un'Goro Crater
    { "Everlook",            1, 53, 60, true,  true  },   // Winterspring
    { "Light's Hope Chapel", 0, 53, 60, true,  true  },   // Eastern Plaguelands
    { "Cenarion Hold",       1, 55, 60, true,  true  },   // Silithus
};

struct ResolvedTown
{
    VanillaTown const* town;
    bool  alliance;
    float x, y, z, o;
};

bool NodeShownTo(TaxiNodesEntry const* node, bool alliance)
{
    if (alliance)
        return node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnAllianceMap) || node->MountCreatureID[1] != 0;
    return node->GetFlags().HasFlag(TaxiNodeFlags::ShowOnHordeMap) || node->MountCreatureID[0] != 0;
}

bool NameIsTown(char const* name, char const* town)
{
    if (!name || !*name) return false;
    size_t const n = std::strlen(town);
    return std::strncmp(name, town, n) == 0 && (name[n] == ',' || name[n] == '\0');
}

// Resolved once (TaxiNodes is loaded before any bot exists; the setup
// pipeline that calls this runs on the world thread).
std::vector<ResolvedTown> const& VanillaTowns()
{
    static std::vector<ResolvedTown> const towns = []
    {
        std::vector<ResolvedTown> out;
        LocaleConstant const locale = ::World::instance()->GetDefaultDbcLocale();   // sWorld would resolve to this namespace
        std::string missing;
        for (VanillaTown const& t : kVanillaTowns)
        {
            for (bool side : { true, false })
            {
                if (side ? !t.alliance : !t.horde) continue;
                TaxiNodesEntry const* found = nullptr;
                for (TaxiNodesEntry const* node : sTaxiNodesStore)
                {
                    if (!node || node->ContinentID != t.map || !NodeShownTo(node, side)) continue;
                    if (!NameIsTown(node->Name[locale], t.town) && !NameIsTown(node->Name[LOCALE_enUS], t.town)) continue;
                    found = node;
                    break;
                }
                if (!found)
                {
                    missing += missing.empty() ? "" : ", ";
                    missing += std::string(t.town) + (side ? " (A)" : " (H)");
                    continue;
                }
                out.push_back({ &t, side, found->Pos.X, found->Pos.Y, found->Pos.Z, found->Facing });
            }
        }
        TC_LOG_INFO("playerbot.v2", "[ZonePicker] Classic: {} vanilla town flight points resolved{}{}",
            uint32(out.size()), missing.empty() ? "" : "; not in TaxiNodes: ", missing);
        return out;
    }();
    return towns;
}

// 1.12 zone level ranges by AreaTable zone id (the ids the Classic client
// and Player::GetZoneId use). Zones with a row in kVanillaTowns use the same
// range here; the racial starter zones are 1-10. Used only to decide that a
// zone has gone grey for a bot (its top level is at/below the bot's grey
// level), never for placement.
struct VanillaZone { uint16 zone; uint8 lo, hi; };
constexpr VanillaZone kVanillaZones[] = {
    {    1,  1, 10 },   // Dun Morogh
    {   12,  1, 10 },   // Elwynn Forest
    {   14,  1, 10 },   // Durotar
    {   85,  1, 10 },   // Tirisfal Glades
    {  141,  1, 10 },   // Teldrassil
    {  215,  1, 10 },   // Mulgore
    { 16593,  1, 10 },   // Zephras Isle (Classic 1.60 Skyborne start, map 2991; quests tuned 1-9, Fleet/SkyborneIsle.h)
    {   38, 10, 20 },   // Loch Modan
    {   40, 10, 20 },   // Westfall
    {  130, 10, 20 },   // Silverpine Forest
    {  148, 11, 20 },   // Darkshore
    {   17, 10, 25 },   // The Barrens
    {   44, 15, 25 },   // Redridge Mountains
    {  406, 15, 27 },   // Stonetalon Mountains
    {  331, 18, 30 },   // Ashenvale
    {   10, 18, 30 },   // Duskwood
    {   11, 20, 30 },   // Wetlands
    {  267, 20, 30 },   // Hillsbrad Foothills
    {  400, 25, 35 },   // Thousand Needles
    {   36, 30, 40 },   // Alterac Mountains
    {   45, 30, 40 },   // Arathi Highlands
    {  405, 30, 40 },   // Desolace
    {   33, 30, 45 },   // Stranglethorn Vale
    {    3, 35, 45 },   // Badlands
    {    8, 35, 45 },   // Swamp of Sorrows
    {   15, 35, 45 },   // Dustwallow Marsh
    {  357, 40, 50 },   // Feralas
    {   47, 40, 50 },   // The Hinterlands
    {  440, 40, 50 },   // Tanaris
    {   51, 43, 50 },   // Searing Gorge
    {   16, 45, 55 },   // Azshara
    {    4, 45, 55 },   // Blasted Lands
    {  361, 48, 55 },   // Felwood
    {  490, 48, 55 },   // Un'Goro Crater
    {   46, 50, 60 },   // Burning Steppes
    {   28, 51, 58 },   // Western Plaguelands
    {   41, 55, 60 },   // Deadwind Pass
    {  139, 53, 60 },   // Eastern Plaguelands
    {  618, 53, 60 },   // Winterspring
    { 1377, 55, 60 },   // Silithus
};

constexpr uint16 kVanillaCapitals[] = { 1519, 1537, 1657, 1637, 1638, 1497 };

} // anonymous

bool VanillaZoneLevelRange(uint32 zone_id, uint8& lo, uint8& hi)
{
    // The ground outside Onyxia's Lair is an area of its own (2159): it is the Dustwallow Marsh entrance, not a town without levels.
    if (zone_id == 2159) zone_id = 15;
    for (VanillaZone const& z : kVanillaZones)
    {
        if (z.zone != zone_id) continue;
        lo = z.lo;
        hi = z.hi;
        return true;
    }
    return false;
}

bool IsVanillaCapitalZone(uint32 zone_id)
{
    for (uint16 c : kVanillaCapitals)
        if (c == zone_id) return true;
    return false;
}

DestinationPick PickVanillaLevelingTown(uint8 level, bool alliance, uint64 seed, uint32 current_map)
{
    std::vector<ResolvedTown const*> here, anywhere;
    for (ResolvedTown const& t : VanillaTowns())
    {
        if (t.alliance != alliance || level < t.town->lo || level > t.town->hi) continue;
        anywhere.push_back(&t);
        if (t.town->map == current_map)
            here.push_back(&t);
    }
    std::vector<ResolvedTown const*> const& pool = here.empty() ? anywhere : here;
    DestinationPick d;
    if (pool.empty())
        return d;
    ResolvedTown const& t = *pool[seed % pool.size()];
    d.map_id = t.town->map;
    d.x = t.x;
    d.y = t.y;
    d.z = t.z;
    d.o = t.o;
    d.zone = t.town->town;
    return d;
}

DestinationPick PickDestination(uint8 level, bool alliance, uint64 seed)
{
    if (::Playerbot::IsVanillaRealm())
    {
        std::vector<ResolvedTown const*> matches;
        for (ResolvedTown const& t : VanillaTowns())
            if (t.alliance == alliance && level >= t.town->lo && level <= t.town->hi)
                matches.push_back(&t);
        if (!matches.empty())
        {
            ResolvedTown const& t = *matches[seed % matches.size()];
            DestinationPick d;
            d.map_id = t.town->map;
            d.x = t.x;
            d.y = t.y;
            d.z = t.z;
            d.o = t.o;
            d.zone = t.town->town;
            return d;   // no expansion gate quest on a vanilla realm
        }
        // Below 10, or no town resolved for this level: the faction capital
        // (never the retail rows below, which reach Outland and beyond).
        DestinationPick d;
        d.map_id = alliance ? 0 : 1;
        d.x = alliance ? -8833.0f : 1633.0f;
        d.y = alliance ?   628.0f :-4439.0f;
        d.z = alliance ?    94.0f :   16.0f;
        d.zone = alliance ? "Stormwind" : "Orgrimmar";
        return d;
    }

    // Collect indices of brackets matching (level, faction). Bracket
    // overlap means dozens of candidates per level — bots distribute
    // widely instead of clustering.
    std::array<size_t, 32> matches{};
    size_t nmatches = 0;
    for (size_t i = 0; i < kBrackets.size(); ++i)
    {
        BracketEntry const& b = kBrackets[i];
        if (level < b.lo || level > b.hi) continue;
        if (b.alliance != alliance) continue;
        if (nmatches < matches.size()) matches[nmatches++] = i;
    }
    if (nmatches == 0)
    {
        // Defensive fallback — should not hit because L0-9 catch-all
        // covers below the lowest leveling bracket.
        DestinationPick d;
        d.map_id = alliance ? 0 : 1;
        d.x = alliance ? -8833.0f : 1633.0f;
        d.y = alliance ?   628.0f :-4439.0f;
        d.z = alliance ?    94.0f :   16.0f;
        d.zone = alliance ? "Stormwind (fallback)" : "Orgrimmar (fallback)";
        return d;
    }
    BracketEntry const& chosen = kBrackets[matches[seed % nmatches]];
    DestinationPick d;
    d.map_id        = chosen.map_id;
    d.x             = chosen.x;
    d.y             = chosen.y;
    d.z             = chosen.z;
    d.zone          = chosen.zone;
    d.starter_quest = chosen.starter_quest;
    return d;
}

namespace
{

// Spots of the hostile, ordinary creatures a bot of each side can level on, built once from the world DB spawns.
std::vector<GrindSpot> const& GrindSpotsFor(bool alliance)
{
    static std::array<std::vector<GrindSpot>, 2> const spots = []
    {
        std::vector<GrindSample> samples[2];
        FactionTemplateEntry const* ft_side[2] = { sFactionTemplateStore.LookupEntry(2), sFactionTemplateStore.LookupEntry(1) };   // [0] horde, [1] alliance
        uint32 mobs = 0;
        for (auto const& [spawn_id, data] : sObjectMgr->GetAllCreatureData())
        {
            MapEntry const* me = sMapStore.LookupEntry(data.mapId);
            if (!me || !me->IsWorldMap() || data.phaseId != 0 || data.phaseGroup != 0)
                continue;
            if (data.spawnGroupData && (data.spawnGroupData->flags & SPAWNGROUP_FLAG_MANUAL_SPAWN))
                continue;
            CreatureTemplate const* t = sObjectMgr->GetCreatureTemplate(data.id);
            if (!t || t->Classification != CreatureClassifications::Normal || t->type == CREATURE_TYPE_CRITTER)
                continue;
            if ((t->flags_extra & CREATURE_FLAG_EXTRA_CIVILIAN) || uint64(data.npcflag.value_or(t->npcflag)) != 0)
                continue;
            CreatureDifficulty const* cd = t->GetDifficulty(DIFFICULTY_NONE);
            if (!cd)
                continue;
            Optional<ContentTuningLevels> lv = sDB2Manager.GetContentTuningData(cd->ContentTuningID, {});
            if (!lv || lv->MinLevel <= 0)
                continue;
            FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(t->faction);
            if (!ft)
                continue;
            ++mobs;
            GrindSample smp{ data.mapId, data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY(), data.spawnPoint.GetPositionZ(),
                uint32((lv->MinLevel + std::max<int>(lv->MinLevel, lv->MaxLevel)) / 2) };
            for (int side = 0; side < 2; ++side)
                if (ft_side[side] && (ft->IsHostileTo(ft_side[side]) || ft_side[side]->IsHostileTo(ft)))
                    samples[side].push_back(smp);
        }
        std::array<std::vector<GrindSpot>, 2> out{ BuildGrindSpots(samples[0]), BuildGrindSpots(samples[1]) };
        TC_LOG_INFO("playerbot.v2", "[grind] {} spawns of ordinary hostile creatures -> {} Horde spots, {} Alliance spots",
            mobs, uint32(out[0].size()), uint32(out[1].size()));
        return out;
    }();
    return spots[alliance ? 1 : 0];
}

} // anonymous

DestinationPick SpreadWithinZone(DestinationPick d, uint8 level, bool alliance, uint64 seed, bool allow_grind)
{
    Map* const map = sMapMgr->FindMap(d.map_id, 0);
    if (!map)
        return d;
    // Half of a fresh crowd goes out to where the mobs are, so the wilds of the zone are populated and not only its towns.
    if (allow_grind && (seed & 1u))
    {
        std::vector<GrindSpot> const& gs = GrindSpotsFor(alliance);
        int const gi = PickGrindSpot(gs, d.map_id, d.x, d.y, level, seed >> 1);
        if (gi >= 0)
        {
            float dx = 0.f, dy = 0.f;
            SpreadOffset(seed, kSpreadMaxOffset * 2.0f, dx, dy);
            float const x = gs[gi].x + dx, y = gs[gi].y + dy;
            float const z = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, gs[gi].z + 6.0f, true, 20.0f);
            bool const wet = z > INVALID_HEIGHT && (map->GetLiquidStatus(PhasingHandler::GetEmptyPhaseShift(), x, y, z + 0.05f, {})
                & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER));
            if (z > INVALID_HEIGHT && !wet)
            {
                d.x = x; d.y = y; d.z = z + 0.5f;
                return d;
            }
        }
    }
    std::vector<SpreadHub> hubs;
    ::Playerbot::Services::Hubs().ForEach([&](::Playerbot::V2::Travel::QuestHub const& h)
    {
        if (h.mapId != d.map_id) return;
        float const dist = std::hypot(h.location.GetPositionX() - d.x, h.location.GetPositionY() - d.y);
        if (dist > kSpreadHubMaxTownDistance) return;
        SpreadHub sh;
        sh.x = h.location.GetPositionX(); sh.y = h.location.GetPositionY(); sh.z = h.location.GetPositionZ();
        sh.radius = h.radius; sh.min_level = h.minLevel; sh.max_level = h.maxLevel; sh.faction_mask = h.factionMask;
        sh.dist_to_town = dist;
        hubs.push_back(sh);
    });
    int const idx = PickSpreadHub(hubs, level, alliance, seed);
    if (idx < 0)
        return d;
    float dx = 0.f, dy = 0.f;
    SpreadOffset(seed, hubs[idx].radius, dx, dy);
    float const x = hubs[idx].x + dx, y = hubs[idx].y + dy;
    // The hub centre averages giver positions, so its height is not trustworthy: stand on the ground found under the spot,
    // searching from just above the hub's own height; no ground there means the spot is not on this hub's floor, keep the town point.
    float const z = map->GetHeight(PhasingHandler::GetEmptyPhaseShift(), x, y, hubs[idx].z + 6.0f, true, 20.0f);
    if (z <= INVALID_HEIGHT)
        return d;
    if (map->GetLiquidStatus(PhasingHandler::GetEmptyPhaseShift(), x, y, z + 0.05f, {}) & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER))
        return d;
    d.x = x; d.y = y; d.z = z + 0.5f;
    return d;
}

} // namespace Playerbot::V2::World
