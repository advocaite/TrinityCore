// WorldBotSpawner - see the header for the model. World thread only.

#include "Fleet/LevelWindow.h"
#include "Api/PlayerbotAPI.h"
#include "Bot/QuestDoable.h"
#include "Bot/Gear/WorldGearStart.h"
#include "Bot/Gear/ArmorSwapGate.h"
#include "Fleet/WorldLeash.h"
#include "Fleet/CityLayer.h"
#include "WorldBotSpawner.h"
#include "Bot/TownLife.h"
#include "GateDuels.h"
#include "Gathering.h"
#include "../Bot/BotChatModel.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "StrangerBoard.h"
#include "SpellAuras.h"
#include "../Bot/SpellFamily.h"
#include "PlayerWorkAreas.h"
#include "BotParties.h"
#include "../Bot/QuestReverseIndex.h"
#include "Bot/World/RaceStartZone.h"
#include "WakeSelect.h"
#include "StrandedWatch.h"
#include "DayRhythm.h"
#include "BotSize.h"
#include "SoakAnchors.h"
#include "BotPool.h"
#include "../Travel/TownNpcIndex.h"
#include "../Travel/ReachMemory.h"
#include "../Travel/SoakBoard.h"
#include "../Travel/UnifiedTravelGraph.h"
#include "UnitDefines.h"
#include "TownGatherSource.h"
#include "WorldFactionMix.h"
#include "Fleet/WorldPvpConfig.h"
#include "SkyborneIsle.h"
#include "BotPopulationManager.h"
#include "BotIdentityRegistry.h"
#include "OwnerRegistry.h"
#include "../Travel/QuestHubDatabase.h"
#include "../Threading/TickScheduler.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"
#include "../Session/BotSessionMgr.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/IdleRule.h"
#include "../Bot/BotIntent.h"
#include "../Bot/IdleWatch.h"
#include "StringFormat.h"
#include "MotionMaster.h"
#include "../Bot/World/ZonePicker.h"
#include "../Bot/World/SpreadSpot.h"
#include "Config.h"
#include "GameTime.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Log.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Map.h"
#include "MapManager.h"
#include "Creature.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "PathGenerator.h"
#include "PlayerbotMovement.h"
#include "Random.h"
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <cmath>
#include <cstdio>
#ifdef _WIN32
#  ifndef PSAPI_VERSION
#    define PSAPI_VERSION 2
#  endif
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#endif

namespace Playerbot::V2::Fleet {

bool IsSoakAnchor(std::string const& name)
{
    static std::mutex mtx;
    static std::unordered_set<std::string> names;
    static uint32 read_ms = 0;
    std::lock_guard<std::mutex> lock(mtx);
    uint32 const now = getMSTime();
    if (!read_ms || getMSTimeDiff(read_ms, now) >= 15000u)
    {
        read_ms = now;
        names = ParseSoakAnchors(sConfigMgr->GetStringDefault("Playerbot.World.SoakAnchors", "", true));
    }
    if (names.empty())
        return false;
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return names.contains(lower);
}

namespace {

constexpr uint32 kConfigReloadMs   = 15000u;
constexpr uint32 kReportMs         = 60000u;
constexpr uint32 kMemReportMs      = 60000u;    // [mem] line
constexpr uint32 kSwapBurstMs = 60000u;          // after a jump the swap runs faster for this long
constexpr uint32 kDrainMaxMs = 600000u;         // a population step down logs the world bots out for at most 10 min (busy ones may stay)
constexpr uint32 kSwapBurstFactor = 3u;          // x spawn and despawn rate during it
constexpr uint32 kPendingTimeoutMs = 120000u;   // login + setup pipeline + placement
constexpr uint32 kQueryTimeoutMs   = 20000u;

float Dist2D(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx, dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

bool IsAllianceRace(uint8 race)
{
    return Player::TeamIdForRace(race) == TEAM_ALLIANCE;
}

// WHERE fragment keeping Skyborne pool rows where they belong (Skyborne::PoolRowFitsAnchor): around a Zephras anchor only Skyborne
// bots of the island's levels, anywhere else no Skyborne bot below level 10 (it sleeps on the island).
std::string SkyborneRaceClause(bool anchor_on_isle)
{
    if (anchor_on_isle)
        return Trinity::StringFormat("AND c.race IN ({}, {}) AND c.level BETWEEN {} AND {} ",
            uint32(Skyborne::kRaceAlliance), uint32(Skyborne::kRaceHorde), uint32(Skyborne::kIsleLevelLo), uint32(Skyborne::kIsleLevelHi));
    return Trinity::StringFormat("AND NOT (c.race IN ({}, {}) AND c.level < {}) ",
        uint32(Skyborne::kRaceAlliance), uint32(Skyborne::kRaceHorde), uint32(Skyborne::kGraduationLevel));
}

} // namespace

// Resident memory of this process in KB; 0 when unavailable.
// Process CPU time (user + kernel), ms; the per-bot CPU figure in the [world] line divides its growth by the awake bots.
static uint64 ProcessCpuMs()
{
#ifdef _WIN32
    FILETIME c{}, e{}, k{}, u{};
    if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u))
    {
        ULARGE_INTEGER ku{}, uu{};
        ku.LowPart = k.dwLowDateTime; ku.HighPart = k.dwHighDateTime;
        uu.LowPart = u.dwLowDateTime; uu.HighPart = u.dwHighDateTime;
        return (ku.QuadPart + uu.QuadPart) / 10000ull;   // 100 ns units
    }
    return 0;
#else
    timespec ts{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        return uint64(ts.tv_sec) * 1000ull + uint64(ts.tv_nsec) / 1000000ull;
    return 0;
#endif
}

static uint64 ProcessRssKb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return uint64(pmc.WorkingSetSize / 1024);
    return 0;
#else
    if (FILE* f = std::fopen("/proc/self/statm", "r"))
    {
        unsigned long size = 0, resident = 0;
        int const n = std::fscanf(f, "%lu %lu", &size, &resident);
        std::fclose(f);
        if (n == 2)
            return uint64(resident) * 4u;   // 4 KB pages
    }
    return 0;
#endif
}

// Physical memory of the machine in MB; 0 when the system will not say.
static uint64 PhysicalRamMb()
{
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return uint64(ms.ullTotalPhys / (1024ull * 1024ull));
    return 0;
#else
    long const pages = sysconf(_SC_PHYS_PAGES);
    long const page  = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0)
        return uint64(pages) * uint64(page) / (1024ull * 1024ull);
    return 0;
#endif
}

// Private (committed, non-shared) bytes in KB; Windows only, 0 elsewhere. The working set can hide memory the OS has paged out.
static uint64 ProcessPrivateKb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return uint64(pmc.PrivateUsage / 1024);
#endif
    return 0;
}

// Counters only, no per-object work beyond one pass over the loaded maps (shared lock, as the core does it).
// Splits the per-bot RAM figure: grids and the creatures/gameobjects they hold vs players vs the rest of the process.
static std::string MemReport(uint32 bots_online)
{
    struct Row { uint32 id; uint32 inst; uint32 grids; uint64 creatures; uint64 gos; uint32 players; };
    std::vector<Row> rows;
    uint64 grids = 0, creatures = 0, gos = 0;
    uint32 players = 0, maps = 0;
    sMapMgr->DoForAllMaps([&](Map* m)
    {
        Row r{ m->GetId(), m->GetInstanceId(), uint32(m->GridRefManager<NGridType>::size()),
               uint64(m->GetObjectsStore().Size<Creature>()), uint64(m->GetObjectsStore().Size<GameObject>()),
               uint32(m->GetPlayers().size()) };
        ++maps;
        grids += r.grids; creatures += r.creatures; gos += r.gos; players += r.players;
        rows.push_back(r);
    });
    std::sort(rows.begin(), rows.end(), [](Row const& a, Row const& b) { return a.creatures + a.gos > b.creatures + b.gos; });
    std::string top;
    for (size_t i = 0; i < rows.size() && i < 8; ++i)
        top += Trinity::StringFormat("{}{}:{}g/{}c/{}o/{}p", i ? " " : "", rows[i].id, rows[i].grids, rows[i].creatures, rows[i].gos, rows[i].players);
    return Trinity::StringFormat(
        "[mem] rss_mb={} private_mb={} | maps={} grids={} creatures={} gameobjects={} players_on_maps={} sessions={} bots_online={} bot_sessions={} registry={} snapshots={} | top maps (id:grids/creatures/gobjs/players) {}",
        ProcessRssKb() / 1024, ProcessPrivateKb() / 1024, maps, grids, creatures, gos, players, ::World::instance()->GetActiveSessionCount(), bots_online,
        Services::SessionMgr().active_count(), Services::Registry().size(), Services::Snapshots().bot_count(), top);
}

WorldBotSpawner::WorldBotSpawner(BotPopulationManager& owner) : owner_(owner) {}

void WorldBotSpawner::LoadConfig()
{
    Config c;
    c.enable           = sConfigMgr->GetBoolDefault  ("Playerbot.World.Enable", false, true);
    c.max_bots         = uint32(sConfigMgr->GetIntDefault("Playerbot.World.MaxBots", 100, true));
    c.bots_per_player  = uint32(sConfigMgr->GetIntDefault("Playerbot.World.BotsPerPlayer", 25, true));
    c.city_bots        = uint32(sConfigMgr->GetIntDefault("Playerbot.World.CityBots", 0, true));
    c.zone_bots        = uint32(sConfigMgr->GetIntDefault("Playerbot.World.ZoneBots", 0, true));
    c.start_zone_bots  = uint32(sConfigMgr->GetIntDefault("Playerbot.World.StartZoneBots", 25, true));
    c.activity_radius  = std::max(50.0f, sConfigMgr->GetFloatDefault("Playerbot.World.ActivityRadius", 300.0f, true));
    c.despawn_radius   = std::max(c.activity_radius * 1.1f,
                                  sConfigMgr->GetFloatDefault("Playerbot.World.DespawnRadius", c.activity_radius * 1.5f, true));
    c.ring_min         = std::max(20.0f, sConfigMgr->GetFloatDefault("Playerbot.World.SpawnRingMin", 110.0f, true));
    c.ring_max         = std::max(c.ring_min + 20.0f, sConfigMgr->GetFloatDefault("Playerbot.World.SpawnRingMax", 200.0f, true));
    c.ring_max         = std::min(c.ring_max, c.activity_radius * 0.9f);
    c.ring_min         = std::min(c.ring_min, c.ring_max - 10.0f);
    c.tick_ms          = std::max<uint32>(500, uint32(sConfigMgr->GetIntDefault("Playerbot.World.TickMs", 1000, true)));
    c.spawn_per_tick   = std::max<uint32>(1, uint32(sConfigMgr->GetIntDefault("Playerbot.World.SpawnPerTick", 2, true)));
    c.global_fill      = sConfigMgr->GetBoolDefault("Playerbot.World.GlobalFill", false, true);
    c.global_per_tick  = std::max<uint32>(1, uint32(sConfigMgr->GetIntDefault("Playerbot.World.GlobalPerTick", 3, true)));
    c.filler_hub_only  = sConfigMgr->GetBoolDefault("Playerbot.World.FillerHubOnly", true, true);
    // Default (-1) is 45% of the machine's RAM, so a small PC stops adding filler bots before it swaps; 0 = no limit.
    int32 const rss_cfg = sConfigMgr->GetIntDefault("Playerbot.World.RssBudgetMb", -1, true);
    c.rss_budget_mb    = rss_cfg >= 0 ? uint32(rss_cfg) : uint32(PhysicalRamMb() * 45u / 100u);
    c.town_pct         = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.TownPct", 100, true)));
    c.city_spawn_per_tick = uint32(sConfigMgr->GetIntDefault("Playerbot.World.CitySpawnPerTick", 0, true));
    c.seed_roster      = sConfigMgr->GetBoolDefault("Playerbot.World.SeedRoster", false, true);
    c.seed_in_flight   = std::max<uint32>(1, uint32(sConfigMgr->GetIntDefault("Playerbot.World.SeedInFlight", 8, true)));
    c.wake_radius      = std::max(0.0f, sConfigMgr->GetFloatDefault("Playerbot.World.WakeRadius", 0.0f, true));
    c.natural_moves    = sConfigMgr->GetBoolDefault("Playerbot.World.NaturalMoves", true, true);
    c.day_rhythm       = sConfigMgr->GetBoolDefault("Playerbot.World.DayRhythm", true, true);
    c.idle_swap        = sConfigMgr->GetBoolDefault("Playerbot.World.IdleSwap", true, true);
    c.idle_swap_strikes = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.IdleSwapStrikes", 2, true), 1, 20));
    c.idle_swap_cd_ms  = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.IdleSwapCooldownSec", 180, true), 30, 3600)) * 1000u;
    c.jump_grace_ms    = uint32(sConfigMgr->GetIntDefault("Playerbot.World.JumpGraceSec", 20, true)) * 1000u;
    c.zone_grace_ms    = uint32(sConfigMgr->GetIntDefault("Playerbot.World.ZoneGraceSec", 300, true)) * 1000u;
    c.despawn_per_tick = std::max<uint32>(1, uint32(sConfigMgr->GetIntDefault("Playerbot.World.DespawnPerTick", 5, true)));
    c.placed_grace_ms  = uint32(sConfigMgr->GetIntDefault("Playerbot.World.PlacedGraceSec", 30, true)) * 1000u;
    c.min_level        = std::clamp<uint32>(uint32(sConfigMgr->GetIntDefault("Playerbot.World.MinLevel", 10, true)), 1, 60);
    c.max_level        = std::clamp<uint32>(uint32(sConfigMgr->GetIntDefault("Playerbot.World.MaxLevel", 60, true)), c.min_level, 60);
    c.friendly_pct     = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.FriendlyPct", 100, true)));
    c.mix.default_pct   = c.friendly_pct;
    c.mix.by_zone       = sConfigMgr->GetBoolDefault("Playerbot.World.ZoneMix", true, true);
    c.mix.capital_pct   = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.CapitalFriendlyPct", 100, true)));
    c.mix.home_pct      = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.HomeFriendlyPct", 100, true)));
    c.mix.border_pct    = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.BorderFriendlyPct", 100, true)));
    c.mix.contested_pct = std::min<uint32>(100, uint32(sConfigMgr->GetIntDefault("Playerbot.World.ContestedFriendlyPct", 100, true)));
    // A PvP or free-for-all realm (GameType) with Playerbot.World.Pvp.Enable: enemy bots about in the border and contested zones
    // (Fleet/WorldPvp.h), so the world PvP of the bots has someone to meet. Capitals and home zones stay as the files say.
    {
        namespace F = ::Playerbot::V2::Fleet;
        F::WorldPvpCfg const pvp = F::ReadWorldPvpCfg();
        F::RealmPvp const realm = F::RealmPvpOf(::World::instance()->getIntConfig(CONFIG_GAME_TYPE));
        c.mix.border_pct = F::PvpBorderPct(pvp, realm, c.mix.border_pct);
        c.mix.contested_pct = F::PvpContestedPct(pvp, realm, c.mix.contested_pct);
    }
    c.pool_max         = uint32(sConfigMgr->GetIntDefault("Playerbot.World.PoolMax", std::max<uint32>(c.max_bots * 4, 200), true));
    c.area_cap_outdoors = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.AreaCapOutdoors", 14, true), 0, 500));
    c.area_cap_town    = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.AreaCapTown", 40, true), 0, 500));
    c.area_cap_start   = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.AreaCapStart", 10, true), 0, 500));
    c.area_radius      = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.AreaRadius", 45.0f, true), 15.0f, 200.0f);
    c.pool_horde_pct   = std::clamp<uint32>(uint32(sConfigMgr->GetIntDefault("Playerbot.World.PoolHordePct", 50, true)), 5, 95);
    c.variable_max     = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.VariableMax", 2000, true), 0, 20000));
    c.travel_soak      = sConfigMgr->GetBoolDefault("Playerbot.World.TravelSoak", false, true);
    c.travel_soak_bots = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.TravelSoakBots", 8, true), 1, 64));
    c.travel_soak_leg_s = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.TravelSoakLegSeconds", 1200, true), 60, 7200));
    c.moving_enable    = sConfigMgr->GetBoolDefault("Playerbot.World.Moving.Enable", true, true);
    c.moving.min_speed = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.MinSpeed", 9.0f, true), 2.0f, 60.0f);
    c.moving.behind_yards = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.BehindYards", 150.0f, true), 0.0f, 2000.0f);
    c.moving.corridor_yards = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.CorridorYards", 250.0f, true), 60.0f, 2000.0f);
    c.moving.lead_sec  = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.LeadSec", 25.0f, true), 2.0f, 120.0f);
    c.moving.lead_min  = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.LeadMin", 130.0f, true), 20.0f, 1000.0f);
    c.moving_spawn_factor = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.Moving.SpawnFactor", 2, true), 1, 8));
    c.moving_despawn_behind = std::clamp(sConfigMgr->GetFloatDefault("Playerbot.World.Moving.DespawnBehindYards", 450.0f, true), 0.0f, 5000.0f);
    c.variable_ttl_h   = uint32(std::clamp<int64>(sConfigMgr->GetIntDefault("Playerbot.World.VariableTtlHours", 24, true), 1, 24 * 90));

    // Plain bot numbers (Fleet/BotSize.h): any Bots.* line replaces the old six keys.
    {
        namespace F = ::Playerbot::V2::Fleet;
        F::PlainInput in;
        bool file_off = false;   // Bots.Size = Off: no world bots from the files (the launcher's Bots window, Off)
        if (std::string const t = sConfigMgr->GetStringDefault("Bots.Size", "", true); !t.empty())
        {
            file_off = F::IsBotSizeOff(t);
            in.size = F::ParseBotSize(t);
        }
        auto num = [](char const* key) -> std::optional<uint32>
        {
            int32 const v = sConfigMgr->GetIntDefault(key, -1, true);
            return v >= 0 ? std::optional<uint32>(uint32(v)) : std::nullopt;
        };
        in.reserve = num("Bots.Reserve");
        in.active_cap = num("Bots.ActiveCap");
        in.in_city = num("Bots.InCity");
        if (std::string const t = sConfigMgr->GetStringDefault("Bots.PerZone", "", true); !t.empty())
            in.per_zone = t;
        std::optional<F::PlainBots> b = F::ResolvePlainBots(in);
        // The ForeverBots panel's choice (a GM, in game) wins over the files: Low / Medium / High is that preset with world bots
        // on, Off is no world bots. While an Off still has bots logging out the spawner keeps running with nothing to fill.
        LoadPopulationOnce();
        // The file's Off is the in-game Off without the logging out: the launcher restarts the realm after the choice, so no world bot
        // is online. Only the world spawner stops; Playerbot.World.Enable stays as it is, so battleground and dungeon finder bots and
        // the player's own bots still come. An in-game pick (below) still wins over it.
        if (file_off && !pop_level_)
        {
            c.enable = false;
            c.seed_roster = false;
            c.global_fill = false;
            if (!b) b = F::PresetBots(F::BotSize::Normal);
        }
        if (pop_level_)
        {
            c.enable = *pop_level_ != F::PopLevel::Off || drain_until_ms_ != 0 || drain_requested_;
            b = F::PopLevelBots(*pop_level_, b ? b->reserve : c.pool_max);
            if (*pop_level_ == F::PopLevel::Off)
            {
                c.seed_roster = false;
                c.global_fill = false;
            }
        }
        if (b)
        {
            c.plain = true;
            c.max_bots = b->active_cap;
            c.pool_max = b->reserve;
            c.city_bots = b->in_city;
            c.zone_lo = b->zone_lo;
            c.zone_hi = b->zone_hi;
            c.zone_bots = b->zone_hi;
            c.bots_per_player = b->zone_hi;                        // fallback and the per-player reserve of the global filler
            c.rss_budget_mb = uint32(PhysicalRamMb() * 45u / 100u);   // memory stop: automatic
            // The numbers are flat (Karrz 11-01: "a flat number"): no quieter night unless the player asks for it. With the old rule a
            // 50-70 zone held 30-42 bots after midnight and a 120 city held 72, so a late player saw a thin world.
            c.day_rhythm = sConfigMgr->GetBoolDefault("Bots.DayRhythm", false, true);
            // Leaving: a zone nobody is in loses its bots after LeaveZoneSec; after a dungeon, battleground or logout the spot is
            // kept as a ghost so the total is LeaveWorldSec (Karrz 11-01: "the bots should log out fast").
            uint32 const leave_zone = uint32(std::clamp<int32>(sConfigMgr->GetIntDefault("Bots.LeaveZoneSec", 20, true), 0, 3600));
            uint32 const leave_world = uint32(std::clamp<int32>(sConfigMgr->GetIntDefault("Bots.LeaveWorldSec", 60, true), 0, 3600));
            c.leave_zone_ms = leave_zone * 1000u;
            c.zone_grace_ms = (leave_world > leave_zone ? leave_world - leave_zone : 0u) * 1000u;
            // Named only when a file gives it a value of its own: the shipped playerbot.conf held the built-in defaults, and the start
            // line then listed six "not used" keys on every install that nobody had ever touched.
            struct OldKey { char const* key; int32 builtin; };
            static constexpr OldKey kOld[] = {
                { "Playerbot.World.MaxBots", 100 }, { "Playerbot.World.BotsPerPlayer", 25 }, { "Playerbot.World.CityBots", 0 },
                { "Playerbot.World.ZoneBots", 0 }, { "Playerbot.World.PoolMax", -2 }, { "Playerbot.World.RssBudgetMb", -1 },
                { "Playerbot.World.ZoneGraceSec", 300 } };
            std::string old;
            for (OldKey const& o : kOld)
                for (std::string const& k : sConfigMgr->GetKeysByString(o.key))
                    if (k == o.key && sConfigMgr->GetIntDefault(o.key, o.builtin, true) != o.builtin)
                        old += (old.empty() ? "" : ", ") + k;
            c.plain_summary = Trinity::StringFormat("Bots: {} - reserve {}, at most {} active, {}-{} per zone, {} in a city; bots leave {} s after a zone empties, "
                                                    "{} s after a dungeon/BG/logout{}{}{}",
                pop_level_ ? std::string("set in game: ") + F::PopLevelWord(*pop_level_) : std::string(in.size ? F::BotSizeName(*in.size) : "own numbers"), c.pool_max, c.max_bots, c.zone_lo, c.zone_hi, c.city_bots, leave_zone, leave_world,
                c.day_rhythm ? "; quieter at night (Bots.DayRhythm = 1)" : "",
                old.empty() ? "" : ". Not used any more (Bots.* lines win): ", old);
            if (file_off && !pop_level_)
                c.plain_summary = "Bots: Off (Bots.Size = Off) - no world bots; own bots, dungeon finder and battleground bots still come "
                                  "(the launcher's Bots window turns them on)";
            if (pop_level_ && *pop_level_ == F::PopLevel::Off)
                c.plain_summary = "Bots: set in game: off - no world bots until the next restart (a GM can turn them on with "
                                  ".bot population low|medium|high; .bot population file hands back to the config files)";
            else if (pop_level_)
                c.plain_summary += " (set in game with .bot population until the next restart; .bot population file hands back to the config files)";
        }
    }
    if (c.plain && c.plain_summary != plain_logged_)
    {
        plain_logged_ = c.plain_summary;
        TC_LOG_INFO("server.loading", "{}", c.plain_summary);
        TC_LOG_INFO("playerbot.v2", "[world] {}", c.plain_summary);
    }
    cfg_ = c;
}

// ---- The world bot choice of the ForeverBots panel (Off / Low / Medium / High) ------------------------------------------------
// Kept in the characters database, one row per realm setting, so a restart keeps what the GM picked in game. The table is made here
// (not a migration file) so the panel works on every install that has this server, whatever migrations it already ran.
namespace
{
    constexpr char const* kPopSettingName = "world_bots";
}

void WorldBotSpawner::LoadPopulationOnce()
{
    if (pop_loaded_)
        return;
    pop_loaded_ = true;
    CharacterDatabase.DirectExecute("CREATE TABLE IF NOT EXISTS playerbot_v2_realm_setting ("
                                    "name VARCHAR(64) NOT NULL, value VARCHAR(64) NOT NULL DEFAULT '', PRIMARY KEY (name)"
                                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    // The world bots are set in the launcher before the start (owner decision 2026-11-04: "just on the launcher"; the add-on's World bots tab is
    // gone). A pick an older build kept from the add-on would win over the launcher with no way left to undo it, so it is dropped here
    // and the config files (Bots.Size, written by the launcher) decide at every start. .bot population still changes it until the
    // next restart.
    if (QueryResult r = CharacterDatabase.Query(Trinity::StringFormat(
            "SELECT value FROM playerbot_v2_realm_setting WHERE name = '{}'", kPopSettingName).c_str()))
    {
        TC_LOG_INFO("server.loading", "World bots: the old in-game pick '{}' is dropped; the launcher's Bots setting (Bots.Size) decides",
                    (*r)[0].GetString());
        CharacterDatabase.DirectExecute(Trinity::StringFormat(
            "DELETE FROM playerbot_v2_realm_setting WHERE name = '{}'", kPopSettingName).c_str());
    }
}

void WorldBotSpawner::SetPopulation(std::optional<PopLevel> level)
{
    namespace F = ::Playerbot::V2::Fleet;
    LoadPopulationOnce();
    // What is allowed now, against what the new choice allows: fewer (or none) means the bots online go first.
    bool const was_on = cfg_.enable;
    uint32 const old_cap = cfg_.max_bots, old_zone = cfg_.plain ? cfg_.zone_hi : std::max(cfg_.zone_bots, cfg_.bots_per_player),
                 old_city = cfg_.city_bots ? cfg_.city_bots : cfg_.bots_per_player;
    pop_level_ = level;
    pop_changed_ = true;
    if (level)
    {
        F::PlainBots const now = F::PopLevelBots(*level, cfg_.pool_max);
        if (was_on && F::PopLevelNeedsDrain(old_cap, old_zone, old_city, now))
            drain_requested_ = true;
        else if (*level != F::PopLevel::Off)
        {
            drain_requested_ = false;   // a step up while a step down is still logging bots out: let them come again
            drain_until_ms_ = 0;
        }
    }
    // Not saved: it lasts until the next restart, then the launcher's setting decides again (LoadPopulationOnce).
    TC_LOG_INFO("playerbot.v2", "[world] population choice: {}", level ? F::PopLevelWord(*level) : "the config files");
}

std::string WorldBotSpawner::PopulationLine() const
{
    namespace F = ::Playerbot::V2::Fleet;
    std::string line;
    if (!pop_level_)
        line = Trinity::StringFormat("World bots: from the config files ({}).", cfg_.enable
            ? Trinity::StringFormat("on, at most {} at once", cfg_.max_bots) : std::string("off"));
    else if (*pop_level_ == F::PopLevel::Off)
        line = "World bots: Off.";
    else
    {
        F::PlainBots const b = F::PopLevelBots(*pop_level_, cfg_.pool_max);
        line = Trinity::StringFormat("World bots: {} - at most {} at once, {}-{} in a zone with you, {} in a capital.",
            *pop_level_ == F::PopLevel::Low ? "Low" : *pop_level_ == F::PopLevel::High ? "High" : "Medium",
            b.active_cap, b.zone_lo, b.zone_hi, b.in_city);
    }
    if (drain_until_ms_ || drain_requested_)
        line += Trinity::StringFormat(" {} world bot(s) still logging out.", ambient_online_);
    return line;
}

// A step down or Off: every world bot that is not busy (no group, instance, battleground or fight) logs out, out of sight where it
// can, a few each tick. Ends when none is left or after kDrainMaxMs; then the new numbers fill (or, for Off, the spawner stops).
void WorldBotSpawner::DriveDrain(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    uint32 left = 0;
    uint32 sent = 0;
    uint32 const limit = cfg_.despawn_per_tick * kSwapBurstFactor;
    std::vector<ObjectGuid> go;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p || !p->IsInWorld()) return;
        if (!owner_.IsAmbientBot(id, p)) return;
        if (p->InBattleground() || p->InBattlegroundQueue() || p->GetGroup()
            || (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())))
            return;
        ++left;
        if (leaving_.contains(uint64(id)) || pending_.contains(uint64(id)) || seeding_.contains(uint64(id))) return;
        if (p->IsInCombat() || p->IsBeingTeleported() || owner_.IsKickProtected(id, now_ms)) return;
        if (go.size() < limit) go.push_back(g);
    });
    for (ObjectGuid const& g : go)
        if (Player* p = ObjectAccessor::FindConnectedPlayer(g))
            if (RetireBot(p, now_ms, anchors))
            {
                ++sent;
                ++n_despawned_;
                placed_.erase(g.GetCounter());
            }
    // Off has no time limit: once it ends the spawner stops, and a bot still online then would stay for good.
    bool const off = pop_level_ && *pop_level_ == ::Playerbot::V2::Fleet::PopLevel::Off;
    bool const timed_out = !off && int32(drain_until_ms_ - now_ms) <= 0;
    if ((left == 0 && pending_.empty() && leaving_.empty()) || timed_out)
    {
        TC_LOG_INFO("playerbot.v2", "[world] population change done after {} s{}", getMSTimeDiff(drain_started_ms_, now_ms) / 1000u,
                    timed_out ? Trinity::StringFormat(" (time limit; {} busy world bot(s) stay until they are free)", left) : std::string());
        drain_until_ms_ = 0;
        pop_changed_ = true;   // Off: read the config again, the spawner now stops
    }
}

WorldLoadFacts WorldBotSpawner::Load() const
{
    WorldLoadFacts f;
    f.awake = ambient_online_;
    int64 const pool = pool_count_ < 0 ? 0 : pool_count_ + variable_count_;   // temporary residents are rows too
    f.sleeping = pool > int64(all_online_) ? uint32(pool - int64(all_online_)) : 0u;
    f.rss_mb = uint32(ProcessRssKb() / 1024u);
    f.cpu_pct = cpu_pct_;
    return f;
}

std::string WorldBotSpawner::Report() const
{
    uint64 const rss = ProcessRssKb();
    double const avg = tick_count_ ? double(tick_cost_sum_ms_) / double(tick_count_) : 0.0;
    // RAM cost per ambient bot: growth over the no-bot baseline, only meaningful with some bots online.
    double per_bot_mb = 0.0;
    // Divided by ALL bots in the world (raid, BG fillers, companions, ambient): the growth over the baseline is paid by
    // all of them, dividing by the ambient ones alone read 15 MB when it was about 5.5 (stage B, 2026-10-07).
    if (all_online_ >= 10 && baseline_rss_kb_ && rss > baseline_rss_kb_)
        per_bot_mb = double(rss - baseline_rss_kb_) / 1024.0 / double(all_online_);
    std::string line = Trinity::StringFormat(
        "[world] enable{} anchors={} (in_instance={}) bots={}/{} (all_bots={} far={} global_settled={} rss_cap={}) pending={} placed={} | skipped no_match={} pool_full={} spawn_failed={} login_failed={} at_target={} (last {}/{}) | logged_in={} created={} placed_total={} "
        "despawned={} abandoned={} | spawner_tick_ms last={} avg={:.2f} max={} | rss_mb={} baseline_mb={} per_bot_mb={:.2f}",
        cfg_.enable ? 1 : 0, anchors_, anchors_in_instance_, ambient_online_, cfg_.max_bots, all_online_, global_online_, n_global_settled_, rss_capped_ ? 1 : 0, pending_.size(), placed_.size(),
        n_no_match_, n_pool_full_, n_spawn_failed_, n_login_failed_, n_at_target_, last_have_, last_target_,
        n_logged_in_, n_created_, n_placed_, n_despawned_, n_abandoned_, last_tick_cost_ms_, avg, max_tick_cost_ms_,
        rss / 1024, baseline_rss_kb_ / 1024, per_bot_mb);
    {
        // The standing roster by level bracket (1-9 / 10s / 20s / 30s / 40s / 50s / 60), Alliance/Horde, as of the last census plus creations since.
        std::string pool;
        for (std::size_t b = 0; b < kPoolBrackets; ++b)
            pool += Trinity::StringFormat(" {}/{}", pool_counts_.alliance[b], pool_counts_.horde[b]);
        line += Trinity::StringFormat(" | pool A/H per bracket{} cap {}", pool, PoolBracketCap(cfg_.pool_max));
    }
    line += Trinity::StringFormat(" | place calls={} none={} rejected no_ground={} water={} no_nav={} lava={} no_path={} area_full={} enemy_town={}",
        n_place_calls_, n_place_none_, n_place_reject_[0], n_place_reject_[1], n_place_reject_[2], n_place_reject_[3], n_place_reject_[4], n_place_reject_[5],
        n_place_reject_[6]);
    {
        uint32 riders = 0;
        for (auto const& [k, t] : motion_)
            if (t.m.moving) ++riders;
        line += Trinity::StringFormat(" | moving riders={} placed_ahead={} woken_ahead={} left_behind={}", riders, n_moving_placed_, n_moving_woken_, n_left_behind_);
    }
    line += Trinity::StringFormat(" | split world={} (cap {}) tied_up={} (group/BG/instance) owned={} (companions) soak_anchors={} evicted_for_room={} | walked_in={} walked_out={} leaving={} idle_swaps={}",
        ambient_online_, cfg_.max_bots, tied_online_, owned_online_, soak_anchors_, n_evicted_, n_arrived_walking_, n_left_walking_, leaving_.size(), n_idle_swaps_);
    {
        auto& tl = ::Playerbot::V2::TownLife::Counters();
        line += Trinity::StringFormat(" | town_life inn_ticks={} inn_walk={} inn_none={} inn_stuck={} mailbox_ticks={} duels={} campfires={} lfg_calls={} idle_strikes town={} field={} gate_duels={} gate_challenges={} errands={} errand_gave_up={} inn_full={} mailbox_full={} player_nudges={}",
            tl.inn_ticks.load(std::memory_order_relaxed), tl.inn_walk.load(std::memory_order_relaxed), tl.inn_none.load(std::memory_order_relaxed), tl.inn_stuck.load(std::memory_order_relaxed), tl.mailbox_ticks.load(std::memory_order_relaxed), tl.duels.load(std::memory_order_relaxed),
            tl.campfires.load(std::memory_order_relaxed), tl.lfg_calls.load(std::memory_order_relaxed), tl.idle_town.load(std::memory_order_relaxed), tl.idle_field.load(std::memory_order_relaxed),
            tl.gate_duels.load(std::memory_order_relaxed), tl.gate_challenges.load(std::memory_order_relaxed),
            tl.errands.load(std::memory_order_relaxed), tl.errand_gave_up.load(std::memory_order_relaxed), tl.inn_full.load(std::memory_order_relaxed),
            tl.mailbox_full.load(std::memory_order_relaxed), tl.player_nudges.load(std::memory_order_relaxed));
    }
    {
        auto& wb = V2::WorkAreas::Board::Get();
        auto const areas = wb.Current();
        line += Trinity::StringFormat(" | share_mobs players={} camp_skips={} quest_skips={} spot_skips={}", areas ? areas->size() : 0u,
            wb.camp_skips.load(std::memory_order_relaxed), wb.quest_skips.load(std::memory_order_relaxed), wb.spot_skips.load(std::memory_order_relaxed));
    }
    {
        auto& kb = V2::Strangers::Board::Get();
        auto const strangers = kb.Current();
        line += Trinity::StringFormat(" | kindness players={} buffs={} heals={} helps={} rezzes={} cooldown_skips={} cast_fails={}",
            strangers ? strangers->size() : 0u, kb.buffs.load(std::memory_order_relaxed), kb.heals.load(std::memory_order_relaxed),
            kb.helps.load(std::memory_order_relaxed), kb.rezzes.load(std::memory_order_relaxed), kb.cooldown_skips.load(std::memory_order_relaxed), kb.cast_fails.load(std::memory_order_relaxed));
        auto& pb = V2::Parties::Board::Get();
        auto const links = pb.Current();
        line += Trinity::StringFormat(" | parties members={} formed={} ended={} assists={} catchups={}", links ? links->size() : 0u,
            parties_formed_, parties_ended_, pb.assists.load(std::memory_order_relaxed), pb.catchups.load(std::memory_order_relaxed));
    }
    auto const tiers = Services::Scheduler().tier_counts();
    // Persistent residents: the pool is the roster, a bot logged out stays where it stood. sleeping = pool bots offline.
    int64 const pool = pool_count_ < 0 ? 0 : pool_count_ + variable_count_;   // temporary residents are rows too
    int64 const sleeping = pool > int64(all_online_) ? pool - int64(all_online_) : 0;
    double const awake_ram_mb = all_online_ ? double(rss) / 1024.0 / double(all_online_) : 0.0;
    line += Trinity::StringFormat(" | roster pool={} awake={} sleeping={} seeding={} seeded_total={} woken_total={} wake_dry={} wake_radius={:.0f} | cost proc_cpu_pct={:.1f} cpu_pct_per_awake_bot={:.3f} ram_mb_per_awake_bot_incl_base={:.2f} ram_mb_per_sleeping_bot=0 (offline: a row in the characters table)",
        pool, all_online_, sleeping, seeding_.size(), n_seeded_, n_woken_, n_wake_dry_, cfg_.wake_radius, cpu_pct_, all_online_ ? cpu_pct_ / double(all_online_) : 0.0, awake_ram_mb);
    {
        auto& rm = ::Playerbot::V2::Travel::ReachMemory::Instance();
        line += Trinity::StringFormat(" | reach_memory cells={} noted={} skipped={}", rm.size(), rm.noted(), rm.skips());
    }
    line += Trinity::StringFormat(" | variable pool={}/{} created={} retired={}", variable_count_, cfg_.variable_max, n_created_variable_, n_retired_);
    line += Trinity::StringFormat(" | tiers combat={} active={} cruise={} idle={} parked={}", tiers[0], tiers[1], tiers[2], tiers[3], tiers[4]);
    return line;
}

// A real player's work area: where he stands and what his open kill and collect objectives need (the creature entries with their
// kill-credit aliases, the items). World bots leave those mobs to him (Fleet/PlayerWorkAreas.h).
static V2::WorkAreas::Area WorkAreaOf(Player* p)
{
    V2::WorkAreas::Area a;
    a.player = p->GetGUID().GetCounter();
    a.group = p->GetGroup() ? p->GetGroup()->GetGUID().GetCounter() : 0;
    a.map = p->GetMapId();
    a.x = p->GetPositionX();
    a.y = p->GetPositionY();
    a.z = p->GetPositionZ();
    for (auto const& [qid, qs] : p->getQuestStatusMap())
    {
        if (qs.Status != QUEST_STATUS_INCOMPLETE) continue;
        Quest const* qt = sObjectMgr->GetQuestTemplate(qid);
        if (!qt) continue;
        for (QuestObjective const& obj : qt->Objectives)
        {
            if (obj.ObjectID <= 0) continue;
            if (obj.Type == QUEST_OBJECTIVE_MONSTER)
            {
                if (p->GetQuestObjectiveData(obj) >= obj.Amount) continue;
                a.kill_entries.push_back(uint32(obj.ObjectID));
                for (uint32 alias : KillCreditAliasesFor(uint32(obj.ObjectID)))
                    a.kill_entries.push_back(alias);
            }
            else if (obj.Type == QUEST_OBJECTIVE_ITEM)
            {
                if (p->GetItemCount(uint32(obj.ObjectID), true) >= uint32(obj.Amount)) continue;
                a.item_entries.push_back(uint32(obj.ObjectID));
            }
        }
    }
    return a;
}

// The real players in the open world, for the world bots' small kindnesses (Fleet/StrangerBoard.h): where they are, whether they
// are dead or losing a fight, which of the bots' buffs they have.
void WorldBotSpawner::PublishStrangers()
{
    if (!Services::Config().kindness_enable())
    {
        V2::Strangers::Board::Get().Publish({});
        return;
    }
    auto& lifecycle = Services::Lifecycle();
    std::vector<V2::Strangers::Stranger> list;
    for (auto const& kv : ::World::instance()->GetAllSessions())
    {
        WorldSession* session = kv.second;
        if (!session) continue;
        Player* p = session->GetPlayer();
        if (!p || !p->IsInWorld() || p->IsBeingTeleported() || lifecycle.is_bot(p->GetGUID().GetCounter())) continue;
        Map const* m = p->GetMap();
        if (!m || m->Instanceable() || p->InBattleground()) continue;
        V2::Strangers::Stranger st;
        st.guid = p->GetGUID().GetCounter();
        st.group = p->GetGroup() ? p->GetGroup()->GetGUID().GetCounter() : 0;
        st.map = p->GetMapId();
        st.x = p->GetPositionX(); st.y = p->GetPositionY(); st.z = p->GetPositionZ();
        st.alliance = p->GetTeam() == ALLIANCE;
        st.level = uint8(p->GetLevel());
        st.alive = p->IsAlive();
        st.ghost = p->HasPlayerFlag(PLAYER_FLAGS_GHOST);
        st.hp_pct = uint8(p->GetMaxHealth() ? std::min<uint64>(100, uint64(p->GetHealth()) * 100 / p->GetMaxHealth()) : 0);
        st.hp = uint32(std::min<uint64>(p->GetHealth(), 0xFFFFFFFFu));
        st.max_hp = uint32(std::min<uint64>(p->GetMaxHealth(), 0xFFFFFFFFu));
        st.has_mana = p->GetMaxPower(POWER_MANA) > 0;
        st.pvp_flagged = p->IsPvP() || p->IsFFAPvP();
        st.pvp_fight = p->duel != nullptr;
        for (auto const& [spell, app] : p->GetAppliedAuras())
        {
            if (!app || !app->IsPositive()) continue;
            if (::Playerbot::SpellFamily::Same(spell, 1243) || ::Playerbot::SpellFamily::Same(spell, 21562)) st.buffs |= V2::Strangers::kFortitude;
            else if (::Playerbot::SpellFamily::Same(spell, 1459) || ::Playerbot::SpellFamily::Same(spell, 23028)) st.buffs |= V2::Strangers::kIntellect;
            else if (::Playerbot::SpellFamily::Same(spell, 1126) || ::Playerbot::SpellFamily::Same(spell, 21849)) st.buffs |= V2::Strangers::kMark;
        }
        for (Unit* a : p->getAttackers())
            if (a && a->IsAlive() && a->IsCharmedOwnedByPlayerOrPlayer())
                st.pvp_fight = true;
        for (Unit* a : p->getAttackers())
            if (a && a->IsAlive() && a->GetTypeId() == TYPEID_UNIT && !a->IsPet() && !a->IsGuardian())
            {
                st.attacker = a->GetGUID().GetCounter();
                st.attacker_level = uint8(a->GetLevel());
                st.attacker_hp_pct = uint8(a->GetMaxHealth() ? std::min<uint64>(100, uint64(a->GetHealth()) * 100 / a->GetMaxHealth()) : 0);
                break;
            }
        list.push_back(st);
    }
    V2::Strangers::Board::Get().Publish(std::move(list));
}

// Small parties of world bots (Fleet/BotParties.h): who is free to team up, where they are and what they fight.
void WorldBotSpawner::PublishParties(uint32 now_ms)
{
    static V2::Parties::Former former;
    if (!Services::Config().parties_enable())
    {
        V2::Parties::Board::Get().Publish({});
        return;
    }
    std::vector<V2::Parties::Bot> bots;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || p->IsBeingTeleported() || p->GetGroup() || !owner_.IsAmbientBot(id, p)) return;
        Map const* m = p->GetMap();
        if (!m || m->Instanceable() || p->InBattleground()) return;
        V2::Parties::Bot b;
        b.guid = p->GetGUID().GetCounter();
        b.map = p->GetMapId();
        b.x = p->GetPositionX(); b.y = p->GetPositionY(); b.z = p->GetPositionZ();
        b.alliance = p->GetTeam() == ALLIANCE;
        b.level = uint8(p->GetLevel());
        b.alive = p->IsAlive();
        b.in_combat = p->IsInCombat();
        if (Unit* v = p->GetVictim(); v && v->GetTypeId() == TYPEID_UNIT && v->IsAlive())
            b.target = v->GetGUID().GetCounter();
        b.can_join = !p->IsMounted() && !p->IsInFlight() && !::Playerbot::V2::World::IsVanillaCapitalZone(p->GetZoneId());
        bots.push_back(b);
    });
    V2::Parties::Settings st;
    st.pct = Services::Config().parties_pct();
    st.max_size = Services::Config().parties_max_size();
    st.life_ms = Services::Config().parties_life_ms();
    auto links = former.Step(bots, st, now_ms);
    parties_formed_ = former.formed();
    parties_ended_ = former.dissolved();
    V2::Parties::Board::Get().Publish(std::move(links));
}

void WorldBotSpawner::CollectAnchors(std::vector<Anchor>& out)
{
    anchors_in_instance_ = 0;
    soak_anchors_ = 0;
    auto& lifecycle = Services::Lifecycle();
    auto add = [&](Player* p)
    {
        Map* m = p->GetMap();
        if (!m) return;
        if (m->IsDungeon() || m->IsBattlegroundOrArena())
        {
            ++anchors_in_instance_;   // world bots stay in the open world: nothing to spawn around a player inside
            return;
        }
        Anchor a;
        a.guid     = p->GetGUID();
        a.map      = p->GetMapId();
        a.x        = p->GetPositionX();
        a.y        = p->GetPositionY();
        a.z        = p->GetPositionZ();
        a.zone     = p->GetZoneId();
        a.level    = uint8(p->GetLevel());
        a.alliance = p->GetTeam() == ALLIANCE;
        out.push_back(a);
    };
    std::vector<V2::WorkAreas::Area> work;   // where each real player is working, for Playerbot.World.ShareMobs
    for (auto const& kv : ::World::instance()->GetAllSessions())
    {
        WorldSession* session = kv.second;
        if (!session) continue;
        Player* p = session->GetPlayer();
        if (!p || !p->IsInWorld() || p->IsBeingTeleported()) continue;
        if (lifecycle.is_bot(p->GetGUID().GetCounter())) continue;   // a bot, not a real person
        add(p);
        if (Services::Config().share_mobs_enable() && p->IsAlive() && p->GetMap() && !p->GetMap()->Instanceable())
            work.push_back(WorkAreaOf(p));
    }
    V2::WorkAreas::Board::Get().Publish(std::move(work));
    // Bots play in headless sessions that the list above does not hold. A configured soak anchor (Fleet/SoakAnchors.h) stands in for a
    // player; no other bot is ever an anchor.
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || p->IsBeingTeleported() || !IsSoakAnchor(p->GetName())) return;
        ++soak_anchors_;
        add(p);
    });
}

uint32 WorldBotSpawner::SpawnRateFor(Anchor const& a) const
{
    bool const town = ::Playerbot::V2::World::IsVanillaCapitalZone(a.zone);
    uint32 const rate = town && cfg_.city_spawn_per_tick ? cfg_.city_spawn_per_tick : cfg_.spawn_per_tick;
    // A rider crosses his bubble in under a minute: the road ahead has to fill as fast as he gets there. At most 8 logins a pass, so a
    // burst of character loads does not stall the world thread (SpawnPerTick above 8 stays as set).
    return a.motion.moving ? std::max(rate, std::min(rate * cfg_.moving_spawn_factor, 8u)) : rate;
}

bool WorldBotSpawner::InBubbleOf(Anchor const& a, float x, float y) const
{
    return InBubble(a.x, a.y, a.motion, x, y, cfg_.activity_radius, cfg_.moving);
}

// Each player's velocity from his position tick to tick (Fleet/MovingBubble.h). On a flight path, or after a jump, he is not moving:
// the corridor is for a player who rides or runs through the world, not one carried over it.
void WorldBotSpawner::TrackMotion(uint32 now_ms, std::vector<Anchor>& anchors)
{
    std::unordered_set<uint64> seen;
    for (Anchor& a : anchors)
    {
        uint64 const k = a.guid.GetCounter();
        seen.insert(k);
        MotionTrack& t = motion_[k];
        Player* p = cfg_.moving_enable ? ObjectAccessor::FindConnectedPlayer(a.guid) : nullptr;
        if (!p || p->IsInFlight() || !t.ms || t.map != a.map)
            t.m = Motion{};
        else
        {
            bool const was = t.m.moving;
            t.m = StepMotion(t.m, a.x - t.x, a.y - t.y, getMSTimeDiff(t.ms, now_ms), cfg_.moving.min_speed);
            if (t.m.moving != was)
                TC_LOG_INFO("playerbot.v2", "[world] {} {} at {:.1f} y/s: {}", p->GetName(), t.m.moving ? "rides" : "slows down",
                            t.m.Speed(), t.m.moving ? "bubble follows his heading" : "bubble is the circle again");
        }
        t.map = a.map; t.x = a.x; t.y = a.y; t.ms = now_ms;
        a.motion = t.m;
    }
    for (auto it = motion_.begin(); it != motion_.end();)
        it = seen.count(it->first) ? std::next(it) : motion_.erase(it);
}

// Plain bot numbers: what one place wants before the cap is shared (city number, the zone's rolled number, the start-zone cap,
// then the hour of the day).
uint32 WorldBotSpawner::PlaceWantFor(Anchor const& a)
{
    namespace F = ::Playerbot::V2::Fleet;
    bool const town = ::Playerbot::V2::World::IsVanillaCapitalZone(a.zone);
    uint32 want = cfg_.city_bots;
    if (!town)
    {
        uint64 const key = F::PlaceKey(a.map, a.zone);
        auto it = zone_roll_.find(key);
        if (it == zone_roll_.end())
            it = zone_roll_.emplace(key, F::ZoneRoll(cfg_.zone_lo, cfg_.zone_hi, urand(0, 0x7FFFFFFF))).first;
        want = it->second;
    }
    bool const start_zone = !town && cfg_.start_zone_bots && ::Playerbot::V2::World::IsStartZone(a.zone);
    if (start_zone)
        return std::min(want, cfg_.start_zone_bots);
    if (!cfg_.day_rhythm)
        return want;
    return ::Playerbot::V2::Fleet::RhythmTarget(want, TimeBreakdown(GameTime::GetGameTime()).tm_hour, cfg_.max_bots);
}

void WorldBotSpawner::PlanPlaces(uint32 now_ms, std::vector<Anchor> const& anchors, std::vector<Anchor> const& keep_anchors)
{
    namespace F = ::Playerbot::V2::Fleet;
    place_target_.clear();
    place_players_.clear();
    if (!cfg_.plain)
        return;
    // A place with a player (or a ghost of one who just went into a dungeon) is occupied now.
    for (Anchor const& a : keep_anchors)
        place_seen_ms_[F::PlaceKey(a.map, a.zone)] = now_ms ? now_ms : 1u;
    // A zone empty for longer than LeaveZoneSec forgets its roll: the next visit picks a new number.
    for (auto it = place_seen_ms_.begin(); it != place_seen_ms_.end();)
    {
        if (getMSTimeDiff(it->second, now_ms) > std::max<uint32>(cfg_.leave_zone_ms, 1000u))
        {
            zone_roll_.erase(it->first);
            it = place_seen_ms_.erase(it);
        }
        else
            ++it;
    }
    std::vector<F::PlaceWant> places;
    for (Anchor const& a : anchors)
    {
        uint64 const key = F::PlaceKey(a.map, a.zone);
        if (place_players_[key]++ == 0)
            places.push_back({ key, PlaceWantFor(a), 1 });
    }
    std::vector<uint32> const got = F::ShareCap(places, cfg_.max_bots);
    for (std::size_t i = 0; i < places.size(); ++i)
        place_target_[places[i].key] = got[i];
}

uint32 WorldBotSpawner::TargetFor(Anchor const& a) const
{
    if (cfg_.plain)
    {
        // The place's share of the cap, split between the players in it (they share its bots).
        uint64 const key = ::Playerbot::V2::Fleet::PlaceKey(a.map, a.zone);
        auto const t = place_target_.find(key);
        auto const n = place_players_.find(key);
        if (t == place_target_.end()) return 0;
        return ::Playerbot::V2::Fleet::PerPlayer(t->second, n == place_players_.end() ? 1u : n->second);
    }

    // A capital is packed (CityBots), a leveling zone gets an ordinary density spread over ActivityRadius (ZoneBots).
    bool const town = ::Playerbot::V2::World::IsVanillaCapitalZone(a.zone);
    uint32 const t = town ? cfg_.city_bots : cfg_.zone_bots;
    uint32 base = t ? t : cfg_.bots_per_player;
    // A start zone is one valley with one camp: the ordinary zone density piled 77 bots into Mulgore (PC 20 Oct), and the whole crowd walked back to the
    // camp's quest giver however often the crowd pass moved it.
    bool const start_zone = !town && cfg_.start_zone_bots && ::Playerbot::V2::World::IsStartZone(a.zone);
    if (start_zone)
        base = std::min(base, cfg_.start_zone_bots);
    // No day rhythm in a start zone: a camp at night is as busy as at noon, or it reads empty (PC 20 Oct: 20 x 80% = 16 bots in the valley).
    if (!cfg_.day_rhythm || start_zone)
        return base;
    // The server machine's hour of the day: quieter at night (less RAM and CPU), busier in the evening (Fleet/DayRhythm.h).
    return ::Playerbot::V2::Fleet::RhythmTarget(base, TimeBreakdown(GameTime::GetGameTime()).tm_hour, cfg_.max_bots);
}

uint32 WorldBotSpawner::CountAmbientNear(Anchor const& a) const
{
    uint32 n = 0;
    // In a capital only bots that stand in the capital count: the 600 y radius also reaches the countryside around
    // it (Elwynn for Stormwind), and bots out there must not fill the city's target.
    bool const town = ::Playerbot::V2::World::IsVanillaCapitalZone(a.zone);
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p || !p->IsInWorld() || p->GetMapId() != a.map) return;
        if (town && p->GetZoneId() != a.zone) return;
        if (!owner_.IsAmbientBot(id, p)) return;
        if (InBubbleOf(a, p->GetPositionX(), p->GetPositionY())) ++n;
    });
    return n;
}

// Awake ambient bots around the anchor by zone: the shares the wake pass tops up (Fleet/WakeSelect.h SelectWakeShared).
std::unordered_map<uint32, uint32> WorldBotSpawner::AwakeByZone(Anchor const& a) const
{
    std::unordered_map<uint32, uint32> out;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || p->GetMapId() != a.map || !owner_.IsAmbientBot(id, p)) return;
        if (InBubbleOf(a, p->GetPositionX(), p->GetPositionY())) ++out[p->GetZoneId()];
    });
    return out;
}

// Lava or slime under (x, y, z): ground a resident must never stand on, whatever the navmesh says about the surface below it.
static bool InMagma(Map* map, PhaseShift const& phase, float x, float y, float z)
{
    LiquidData ld;
    ZLiquidStatus const st = map->GetLiquidStatus(phase, x, y, z, {}, &ld);
    if (!(st & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER | LIQUID_MAP_WATER_WALK)))
        return false;
    return ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) || ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime);
}

// A complete navmesh path from `from` (standing where it is) to (x, y, z). A map without a mesh gives no verdict: reachable.
static bool CompletePathTo(Player* from, float x, float y, float z)
{
    PathGenerator pg(from);
    ::Playerbot::BotMovement::SehSafeCalculatePath(pg, x, y, z);
    PathType const t = pg.GetPathType();
    if (t & (PATHFIND_NOT_USING_PATH | PATHFIND_SHORTCUT))
        return true;
    return (t & PATHFIND_NORMAL) && !(t & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_FARFROMPOLY));
}

// The same from (sx, sy, sz) instead of where `ref` stands; `ref` gives the map and the phase.
static bool CompletePathBetween(Player* ref, float sx, float sy, float sz, float x, float y, float z)
{
    PathGenerator pg(ref);
    ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, sx, sy, sz, x, y, z);
    PathType const t = pg.GetPathType();
    if (t & (PATHFIND_NOT_USING_PATH | PATHFIND_SHORTCUT))
        return true;
    return (t & PATHFIND_NORMAL) && !(t & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_FARFROMPOLY)) && !PathHasCliff(pg.GetPath());
}

// Is there a way from where `from` stands to (x, y, z)? One path call gives up after 74 polygons and calls the rest INCOMPLETE, which in a
// winding city (Stormwind's canals and stairs, Orgrimmar's ramps) is no more than "far": 231 healthy bots an hour were rescued as
// stranded on that alone. So an incomplete path is followed: the next call starts where the last one ended, and the goal is reached when
// some call completes. A real pocket (the Great Forge pit) ends at the same point every time, so the chain stops advancing and it is
// refused. At most `hops` calls; `used` gets the number made (the caller's probe budget).
// A path that goes down a cliff face is not one a bot can walk: Detour joins the layers of Stormwind over steep faces, so a bot on a
// platform above the city "reaches" the city by a drop it cannot make (Dutchess, 36 minutes on the wall at z 101). The check is
// Fleet::PathHasCliff (CityLayer.h); no function of the same name may be declared in this file, or the call below binds to it.
static bool ReachableChain(Player* from, float x, float y, float z, uint32 hops, uint32& used)
{
    float sx = from->GetPositionX(), sy = from->GetPositionY(), sz = from->GetPositionZ();
    for (uint32 h = 0; h < hops; ++h)
    {
        ++used;
        PathGenerator pg(from);
        ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, sx, sy, sz, x, y, z);
        PathType const t = pg.GetPathType();
        if (t & (PATHFIND_NOT_USING_PATH | PATHFIND_SHORTCUT))
            return true;
        if ((t & PATHFIND_NORMAL) && !(t & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_FARFROMPOLY)))
            return !PathHasCliff(pg.GetPath());
        if (!(t & PATHFIND_INCOMPLETE) || (t & PATHFIND_NOPATH))
            return false;
        Movement::PointsArray const& pts = pg.GetPath();
        if (pts.size() < 2 || PathHasCliff(pts))
            return false;
        G3D::Vector3 const& end = pts.back();
        if (std::hypot(end.x - sx, end.y - sy) < 3.0f)
            return false;   // no progress: the path ends where it began
        sx = end.x; sy = end.y; sz = end.z;
    }
    return false;
}

// A sleeper saved in a crowd (the roster was once placed on one flight master point per town) must not wake into it. It wakes at a
// quest hub of the same zone that suits its level, chosen anew each time, or failing that a few yards off the spot, on the
// ground. A bot with nobody around it stays exactly where it slept.
void WorldBotSpawner::StepOffStack(Player* bot)
{
    // A bot saved on a flight master node stands a couple of yards BELOW the ground (the node height is under the floor mesh): it
    // is off the navmesh, every path from there fails in place. Lift it onto the ground first, whatever the crowd.
    if (bot->GetMap() && !bot->IsInCombat() && bot->IsAlive())
    {
        float const gz = bot->GetMap()->GetHeight(bot->GetPhaseShift(), bot->GetPositionX(), bot->GetPositionY(),
                                                  bot->GetPositionZ() + 3.0f, true, 6.0f);
        if (gz > INVALID_HEIGHT && gz - bot->GetPositionZ() > 0.3f && gz - bot->GetPositionZ() <= 4.0f)
        {
            Playerbot::BotMovement::SafeNearTeleport(bot, bot->GetPositionX(), bot->GetPositionY(), gz + 0.3f, bot->GetOrientation());
            TC_LOG_INFO("playerbot.v2", "[world] {} woke {:.1f} y under the ground, lifted onto it", bot->GetName(), gz - bot->GetPositionZ());
        }
    }
    // A sleeper saved off the navmesh (over a lava pit, inside a wall, on a roof: a spot an older build placed it on) cannot path
    // anywhere and stands there for good. Move it to the nearest walkable ground, if there is any close by.
    if (bot->GetMap() && !bot->IsInCombat() && bot->IsAlive())
    {
        Position onmesh;
        float const px = bot->GetPositionX(), py = bot->GetPositionY(), pz = bot->GetPositionZ();
        if (!::Playerbot::BotMovement::NearestNavPoint(bot, px, py, pz, 3.0f, 4.0f, onmesh))
        {
            Position wide;
            if (::Playerbot::BotMovement::NearestNavPoint(bot, px, py, pz, 40.0f, 12.0f, wide))
            {
                Playerbot::BotMovement::SafeNearTeleport(bot, wide.GetPositionX(), wide.GetPositionY(), wide.GetPositionZ() + 0.3f,
                                                         bot->GetOrientation(), /*navmesh_point*/ true);
                TC_LOG_INFO("playerbot.v2", "[world] {} woke off the navmesh at ({:.0f},{:.0f},{:.0f}), moved {:.0f} y onto walkable ground",
                            bot->GetName(), px, py, pz, std::hypot(wide.GetPositionX() - px, wide.GetPositionY() - py));
            }
            else
                TC_LOG_INFO("playerbot.v2", "[world] {} woke off the navmesh at ({:.0f},{:.0f},{:.0f}), no walkable ground within 40 y",
                            bot->GetName(), px, py, pz);
        }
    }
    constexpr float kCrowdYards = 8.0f;
    uint32 near_bots = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (uint64(id) == bot->GetGUID().GetCounter()) return;
        Player* other = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (other && other->IsInWorld() && other->GetMapId() == bot->GetMapId() && other->GetDistance(bot) < kCrowdYards)
            ++near_bots;
    });
    if (near_bots < 2 || bot->IsInCombat() || !bot->IsAlive() || !bot->GetMap())
        return;
    uint64 const seed = bot->GetGUID().GetCounter() + uint64(n_woken_) * 7919u;
    ::Playerbot::V2::World::DestinationPick here;
    here.map_id = bot->GetMapId(); here.x = bot->GetPositionX(); here.y = bot->GetPositionY(); here.z = bot->GetPositionZ();
    here = ::Playerbot::V2::World::SpreadWithinZone(here, bot->GetLevel(), bot->GetTeam() == ALLIANCE, seed);
    if (std::fabs(here.x - bot->GetPositionX()) < 0.5f && std::fabs(here.y - bot->GetPositionY()) < 0.5f)
    {
        float dx = 0.f, dy = 0.f;
        ::Playerbot::V2::World::SpreadOffset(seed, 24.0f, dx, dy);
        float const x = bot->GetPositionX() + dx, y = bot->GetPositionY() + dy;
        float const z = bot->GetMap()->GetHeight(bot->GetPhaseShift(), x, y, bot->GetPositionZ() + 3.0f, true, 8.0f);
        if (z <= INVALID_HEIGHT || std::fabs(z - bot->GetPositionZ()) > 6.0f)
            return;   // not the same floor: stay where it slept
        if (bot->GetMap()->GetLiquidStatus(bot->GetPhaseShift(), x, y, z + 0.05f, {}) & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER))
            return;
        here.x = x; here.y = y; here.z = z + 0.5f;
    }
    Playerbot::BotMovement::SafeNearTeleport(bot, here.x, here.y, here.z, bot->GetOrientation());
}

// A capital takes a crowd, a start zone (one camp, one quest giver for every new character) the smallest, the rest of the world between.
uint32 WorldBotSpawner::AreaCapFor(uint32 zone) const
{
    if (::Playerbot::V2::World::IsVanillaCapitalZone(zone))
        return cfg_.area_cap_town;
    if (::Playerbot::V2::World::IsStartZone(zone) && cfg_.area_cap_start)
        return cfg_.area_cap_start;
    return cfg_.area_cap_outdoors;
}

bool WorldBotSpawner::PlaceNear(Player* bot, Player* anchor)
{
    ++n_place_calls_;
    // A start zone takes only the races that start there (a troll is not put in Deathknell).
    if (!::Playerbot::V2::World::RaceFitsStartZone(bot->GetRace(), bot->GetLevel(), anchor->GetZoneId()))
        return false;
    Map* map = anchor->GetMap();
    if (!map) return false;

    // In a capital the districts are small and the player sees a few streets, not 110 y of open ground: use a tighter ring
    // so the crowd stays in the district the player stands in (the LOS preference below still hides the spawn itself).
    bool const in_town = ::Playerbot::V2::World::IsVanillaCapitalZone(anchor->GetZoneId());
    // A start zone is one valley and one camp: its residents are placed around the player as in a capital, not 110-200 y out (PC 20 Oct: the nearest
    // world bot of a level 2 druid in Mulgore stood 139 y away and the camp looked empty).
    bool const tight_ring = in_town || ::Playerbot::V2::World::IsStartZone(anchor->GetZoneId());
    float const ring_min = tight_ring ? std::min(cfg_.ring_min, 45.0f) : cfg_.ring_min;
    float const ring_max = tight_ring ? std::min(cfg_.ring_max, 110.0f) : cfg_.ring_max;

    // Spawn spots: quest hubs (and, in a capital, the hubs of the town) around the anchor, so the crowd stands where
    // bots actually live and then goes about its own business there. A ring spot around the player is only the
    // fallback; a crowd that is always rebuilt on a ring reads as bots following the player.
    struct Spot { float x, y, z, r, d; float w = 1.0f; };
    Anchor self;   // the anchor as a plain record, to count the bots already around him
    self.guid = anchor->GetGUID(); self.map = anchor->GetMapId(); self.x = anchor->GetPositionX(); self.y = anchor->GetPositionY();
    self.zone = anchor->GetZoneId();
    if (auto const mt = motion_.find(anchor->GetGUID().GetCounter()); mt != motion_.end())
        self.motion = mt->second.m;
    // A rider (Fleet/MovingBubble.h): every spot is on the road ahead of him, where he will be in 10-25 s. Not in a capital or a start
    // zone, whose crowd stays around the places people stand.
    bool const riding = self.motion.moving && !tight_ring;
    float lead_lo = 0.0f, lead_hi = 0.0f;
    if (riding)
        LeadRange(self.motion, cfg_.activity_radius, cfg_.moving, lead_lo, lead_hi);
    auto ahead = [&](float x, float y)
    {
        float along = 0.0f, side = 0.0f;
        AlongSide(self.x, self.y, self.motion.vx, self.motion.vy, x, y, along, side);
        return along >= lead_lo * 0.5f && along <= lead_hi && side <= cfg_.moving.corridor_yards * 0.6f;
    };
    bool const arriving = CountAmbientNear(self) < 20;
    // The bot's own side, not the player's: an enemy bot around a Horde player in the Crossroads used the Crossroads' inn and flight
    // master as its meeting place and died to the guards there (Discord 2026-11-10).
    bool const bot_alliance = bot->GetTeam() == ALLIANCE;
    std::vector<Spot> hubs, near_hubs;   // near_hubs: within ~150 y of the player (capitals favour them)
    {
        float const ax = anchor->GetPositionX(), ay = anchor->GetPositionY();
        float const min_d = tight_ring ? 35.0f : 60.0f;
        // A capital: where people gather (bank, auction house, inn, flight master, battlemaster), not the quest-giver hubs, which in
        // Ironforge are the ward entrance rooms.
        // Playerbot.World.Questing = 0: outside a city too the crowd stands where people meet (an inn, a flight master: Goldshire, Crossroads),
        // not at the quest givers, which only matter to a bot that quests; the rest of the crowd gets a ring spot among the mobs and grinds.
        bool const grind_world = !Services::Config().world_questing();
        if (in_town || grind_world)
            for (::Playerbot::V2::Fleet::GatherSpot const& g : ::Playerbot::V2::Fleet::TownGatherSpots(
                     anchor->GetMapId(), ax, ay, riding ? lead_hi : std::min(cfg_.activity_radius * 0.9f, 150.0f), bot_alliance))
            {
                float const d = Dist2D(g.x, g.y, ax, ay);
                if (d < min_d * 0.5f) continue;
                if (riding && !ahead(g.x, g.y)) continue;
                Spot const sp{ g.x, g.y, g.z, ::Playerbot::V2::Fleet::kGatherRadius, d, g.weight };
                hubs.push_back(sp);
                if (d <= 150.0f) near_hubs.push_back(sp);
            }
        if (!(in_town && !hubs.empty()))
        Services::Hubs().ForEach([&](::Playerbot::V2::Travel::QuestHub const& h)
        {
            if (h.mapId != anchor->GetMapId()) return;
            if (grind_world && !in_town) return;
            if (in_town && h.zoneId != anchor->GetZoneId()) return;   // a city's crowd stays in the city, not in the hubs of the countryside around it
            // Outside a town a resident is put where it has something to do: a hub whose quests it cannot take (level, an earlier quest of the
            // chain, class) held seven starter bots at Grelin Whitebeard for 20 minutes, refusing every offer. No such hub: the ring spots below.
            if (!in_town && !::Playerbot::HubHasDoableQuest(bot, h)) return;
            // Outside a city a bot stands at a hub to take its quests. A hub where this bot can take none (the chain starts are done, the quests are
            // above its level or another class's) only parks it at the quest giver with nothing to do, and a valley's one hub then holds the whole crowd
            // (Coldridge camp, 2026-10-18): the bot gets a ring spot among the mobs instead, where it grinds.
            if (!in_town && !::Playerbot::HubHasDoableQuest(bot, h)) return;
            float const d = Dist2D(h.location.GetPositionX(), h.location.GetPositionY(), ax, ay);
            if (riding && !ahead(h.location.GetPositionX(), h.location.GetPositionY())) return;
            if (d >= min_d && d <= cfg_.activity_radius * 0.9f)
            {
                Spot const sp{ h.location.GetPositionX(), h.location.GetPositionY(), h.location.GetPositionZ(), std::min(h.radius, 40.0f), d };
                hubs.push_back(sp);
                if (d <= 150.0f) near_hubs.push_back(sp);
            }
        });
    }

    // Where the residents of this map already stand: a hub pick prefers the less crowded of a few, and a spot already at the area cap
    // is refused (Fleet/AreaCap.h). A town's hubs near the player are few, and "the closest ones first" put the whole crowd in one doorway.
    uint32 const area_cap = AreaCapFor(anchor->GetZoneId());
    std::vector<::Playerbot::V2::Fleet::CrowdPoint> crowd;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (uint64(id) == bot->GetGUID().GetCounter()) return;
        Player* o = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (o && o->IsInWorld() && o->GetMapId() == anchor->GetMapId() && owner_.IsAmbientBot(id, o))
        {
            // A bot just sent here walks in from out of sight: count it at the spot it is walking to. By its position on the way every
            // bot looked like room at the same hub, they all arrived together and the crowd pass sent the extras off again (every 10 s).
            float cx = o->GetPositionX(), cy = o->GetPositionY();
            if (auto const it = place_dest_.find(uint64(id)); it != place_dest_.end())
            {
                if (int32(it->second.until_ms - getMSTime()) > 0) { cx = it->second.x; cy = it->second.y; }
                else place_dest_.erase(it);
            }
            crowd.push_back({ uint64(id), cx, cy, area_cap });
        }
    });
    auto crowd_at = [&](Spot const& h)
    {
        return ::Playerbot::V2::Fleet::CountWithin(crowd, h.x, h.y, h.r + 15.0f);
    };

    float bx = 0, by = 0, bz = 0;
    bool have = false, hidden = false;
    for (int attempt = 0; attempt < 12 && !hidden; ++attempt)
    {
        float x, y, dist;
        // The level the ground is looked for at: the player's own, or a hub's. A city on two levels (the Undercity under the Ruins of
        // Lordaeron, joined only by its lifts) has its bank, auction house and inn 105 y under a player in the ruins; probed from his
        // height the spot over each hub was the courtyard's flagstones, so every resident of the city stood in the courtyard (live 0.1.25:
        // 398 of 428 placements in the Undercity zone at z 40 or more).
        float probe_z = anchor->GetPositionZ();
        bool at_hub = false;
        Spot hub{};
        // Capital: 60% a quest-giver hub near the player (the busy spots), 25% a spot on a tight ring, 15% any hub of the
        // city, so the crowd is around him and not in the far districts. Elsewhere: any hub of the zone.
        uint32 const roll = urand(0, 99);
        // The first eight tries stand at a hub; when every hub near the player is full (a starter valley has one or two) the last four
        // take a ring spot, instead of the whole pass failing with area_full.
        bool const use_hub = !hubs.empty() && attempt < 8 && (!in_town || (roll < 60 && !near_hubs.empty()) || roll >= 85);
        if (use_hub)
        {
            std::vector<Spot> const& base = (in_town && roll < 60 && !near_hubs.empty()) ? near_hubs : hubs;
            // Arrival: while fewer than 20 bots stand around the player, the first ones go to the hubs closest to him, so
            // the nearest ones appear first; the rest spread as usual.
            std::vector<Spot> pool = base;
            if (arriving && pool.size() > 8)
            {
                std::partial_sort(pool.begin(), pool.begin() + 8, pool.end(), [](Spot const& l, Spot const& r) { return l.d < r.d; });
                pool.resize(8);
            }
            // In a town: of three hubs drawn at random, the one with the fewest residents already around it.
            // Weighted by what gathers there (bank and auction house over a lone vendor), then the least crowded for its weight.
            auto draw = [&]() -> Spot const*
            {
                float total = 0.0f;
                for (Spot const& sp : pool) total += sp.w;
                float t = frand(0.0f, total);
                for (Spot const& sp : pool)
                {
                    t -= sp.w;
                    if (t < 0.0f) return &sp;
                }
                return &pool.back();
            };
            Spot const* pick = draw();
            if (in_town)
                for (int k = 0; k < 2; ++k)
                {
                    Spot const* alt = draw();
                    if (float(crowd_at(*alt)) / alt->w < float(crowd_at(*pick)) / pick->w)
                        pick = alt;
                }
            Spot const& h = *pick;
            if (in_town) { probe_z = h.z; hub = h; at_hub = true; }
            float const a = frand(0.0f, 2.0f * float(M_PI));
            float const rr = frand(0.0f, h.r);
            x = h.x + std::cos(a) * rr;
            y = h.y + std::sin(a) * rr;
            dist = Dist2D(x, y, anchor->GetPositionX(), anchor->GetPositionY());
        }
        else if (riding)
        {
            LeadSpot(anchor->GetPositionX(), anchor->GetPositionY(), self.motion, cfg_.activity_radius, cfg_.moving,
                     frand(0.0f, 1.0f), frand(0.0f, 1.0f), x, y);
            dist = Dist2D(x, y, anchor->GetPositionX(), anchor->GetPositionY());
        }
        else
        {
            float const ang = frand(0.0f, 2.0f * float(M_PI));
            dist = frand(ring_min, ring_max);
            x = anchor->GetPositionX() + std::cos(ang) * dist;
            y = anchor->GetPositionY() + std::sin(ang) * dist;
        }
        float const z = map->GetHeight(anchor->GetPhaseShift(), x, y, probe_z + 20.0f, true, 60.0f);
        if (z <= INVALID_HEIGHT)
        {
            ++n_place_reject_[0];
            continue;
        }
        // The ground height of a canal or pond is its floor: never put a bot on a spot that has water over it (even ankle-deep: the check is at the floor, not at head height).
        if (map->GetLiquidStatus(anchor->GetPhaseShift(), x, y, z + 0.05f, {}) & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER))
        {
            ++n_place_reject_[1];
            continue;
        }
        // The height probe finds ANY surface within 60 y below the player: the floor of a lava pit (Ironforge's Great Forge), a roof,
        // a lower level. Stand only where a walkable navmesh polygon is, so the bot can path from its spot to anywhere.
        Position nav;
        if (!::Playerbot::BotMovement::NearestNavPoint(anchor, x, y, z, 4.0f, 5.0f, nav))
        {
            ++n_place_reject_[2];
            continue;
        }
        // Walkable is not yet "a place to live": never lava or slime, and in a town a spot the player can walk to. The Great Forge's
        // pit floor is ground to the probe and to the mesh but joins nothing; a bot put there stands in it for good.
        if (InMagma(map, anchor->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 0.05f))
        {
            ++n_place_reject_[3];
            continue;
        }
        // In a town, a spot the player can walk to; a hub's spot may instead lie on the hub's own level, where it must reach the hub (the
        // city's place it stands at, not a pocket under it).
        if (in_town && !CompletePathTo(anchor, nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ())
            && !(at_hub && CompletePathBetween(anchor, nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ(), hub.x, hub.y, hub.z)))
        {
            ++n_place_reject_[4];
            continue;
        }
        // Never in a town of the other side: its guards attack the bot on sight (Alliance bots placed around a Horde player in the
        // Crossroads died there one after another, Discord 2026-11-10).
        if (::Playerbot::V2::Fleet::InHostileTown(anchor->GetMapId(), nav.GetPositionX(), nav.GetPositionY(), bot_alliance))
        {
            ++n_place_reject_[6];
            continue;
        }
        // The cap of the zone the spot is in, as the crowd pass (DriveCrowd) counts it: the Undercity's courtyard is partly Tirisfal
        // ground, where the outdoor cap holds. Placed under the town cap of the player's zone, the courtyard took more than the crowd pass
        // allowed there, which sent the extras back to the player, into the same courtyard (0.1.25: 102 crowd moves in an hour).
        uint32 const spot_cap = AreaCapFor(map->GetZoneId(anchor->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ()));
        if (!::Playerbot::V2::Fleet::RoomFor(crowd, nav.GetPositionX(), nav.GetPositionY(), cfg_.area_radius, spot_cap))
        {
            ++n_place_reject_[5];   // the area is full: another spot
            continue;
        }
        bx = nav.GetPositionX(); by = nav.GetPositionY(); bz = nav.GetPositionZ();
        last_place_dist_ = dist;
        have = true;
        // Prefer a spot the anchor cannot see, so bots do not pop into view; keep the last candidate otherwise. A spot ahead of a rider
        // beyond the visibility range is out of his sight too.
        hidden = (riding && dist > map->GetVisibilityRange()) || !anchor->IsWithinLOS(bx, by, bz + 1.5f);
    }
    if (!have)
    {
        ++n_place_none_;
        // Nothing was ever placed and the navmesh refused the spots: the world's mmaps are not there (a repack ships only the dungeon ones;
        // setup\build-bot-pathfinding.bat builds the rest). Say so once a minute, so the empty world has a reason in the log.
        static uint32 s_nonav_warn_ms = 0;
        if (n_placed_ == 0 && n_place_reject_[2] > 0 && getMSTimeDiff(s_nonav_warn_ms, getMSTime()) >= 60000u)
        {
            s_nonav_warn_ms = getMSTime();
            TC_LOG_ERROR("playerbot.v2", "[world] WARNING: no navmesh found around the player on map {} ({} spots refused, no bot placed so far). World bots cannot be placed "
                "without the world mmaps: run setup\\build-bot-pathfinding.bat (or copy the mmaps folder) and restart.", anchor->GetMapId(), n_place_reject_[2]);
        }
        return false;
    }

    float const o = frand(0.0f, 2.0f * float(M_PI));
    // A spot the player can see: the bot does not pop into view there. It appears out of his sight, 110-160 y away, and walks in.
    float ex = 0.0f, ey = 0.0f, ez = 0.0f;
    bool arrive_on_foot = false;
    if (cfg_.natural_moves && !hidden && !riding)   // a rider meets the bot on his road; it does not walk in to him
    {
        for (int k = 0; k < 10 && !arrive_on_foot; ++k)
        {
            float const ang = frand(0.0f, 2.0f * float(M_PI));
            float const d = frand(110.0f, 160.0f);
            float const x = anchor->GetPositionX() + std::cos(ang) * d, y = anchor->GetPositionY() + std::sin(ang) * d;
            if (Dist2D(x, y, bx, by) > 220.0f) continue;
            float const z = map->GetHeight(anchor->GetPhaseShift(), x, y, anchor->GetPositionZ() + 20.0f, true, 60.0f);
            if (z <= INVALID_HEIGHT) continue;
            Position nav;
            if (!::Playerbot::BotMovement::NearestNavPoint(anchor, x, y, z, 4.0f, 5.0f, nav)) continue;
            if (InMagma(map, anchor->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 0.05f)) continue;
            if (map->GetLiquidStatus(anchor->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 0.05f, {})
                & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) continue;
            if (anchor->IsWithinLOS(nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 1.5f)) continue;
            if (in_town && !CompletePathTo(anchor, nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ())) continue;
            if (::Playerbot::V2::Fleet::InHostileTown(anchor->GetMapId(), nav.GetPositionX(), nav.GetPositionY(), bot_alliance)) continue;
            ex = nav.GetPositionX(); ey = nav.GetPositionY(); ez = nav.GetPositionZ();
            arrive_on_foot = true;
        }
    }
    float const sx = arrive_on_foot ? ex : bx, sy = arrive_on_foot ? ey : by, sz = arrive_on_foot ? ez : bz;
    bool const moved = bot->GetMapId() == anchor->GetMapId()
        ? ::Playerbot::BotMovement::SafeNearTeleport(bot, sx, sy, sz, o)
        : ::Playerbot::BotMovement::SafeTeleport(bot, anchor->GetMapId(), sx, sy, sz, o, /*options*/ 0);
    last_place_x_ = sx; last_place_y_ = sy; last_place_z_ = sz;
    if (moved)
        place_dest_[bot->GetGUID().GetCounter()] = PlaceDest{ bx, by, getMSTime() + 90000u };
    if (moved && riding)
        ++n_moving_placed_;
    if (moved && arrive_on_foot)
        if (BotAI* ai = Services::Registry().ai(bot->GetGUID().GetCounter()))
        {
            ai->walk_order().Set(::Playerbot::V2::WalkOrder::Arrive, bx, by, bz);
            ++n_arrived_walking_;
        }
    return moved;
}

void WorldBotSpawner::LoadGearResetPending()
{
    if (gear_reset_loaded_ || gear_reset_inflight_)
        return;
    gear_reset_inflight_ = true;
    queries_.AddCallback(CharacterDatabase.AsyncQuery("SELECT character_guid_low FROM playerbot_v2_character WHERE world_gear_pending = 1")
        .WithCallback([this](QueryResult res)
    {
        if (res)
            do { gear_reset_pending_.insert(res->Fetch()[0].GetUInt64()); } while (res->NextRow());
        gear_reset_loaded_ = true;
        gear_reset_inflight_ = false;
        if (!gear_reset_pending_.empty())
            TC_LOG_INFO("playerbot.v2", "[world] {} world bots under level 10 will be set back to their starter outfit on login", gear_reset_pending_.size());
    }));
}

// A world bot made before build 109 wears a generated full set at level 1-9. It is stripped once to the outfit its character was created with, and the
// row is marked so it never happens again; one that has reached level 10 since keeps what it wears (only the mark is cleared).
void WorldBotSpawner::ResetStarterGearOnce(Player* bot)
{
    uint64 const guid_low = bot->GetGUID().GetCounter();
    if (!gear_reset_pending_.contains(guid_low) || gear_reset_tried_.contains(guid_low))
        return;
    // Armor cannot be put on while casting or fighting (result 40, "can't do that right now"): try again on a later tick.
    if (!::Playerbot::Gear::CanSwapArmorNow(bot->IsInCombat(), bot->HasUnitState(UNIT_STATE_CASTING) || bot->IsNonMeleeSpellCast(false)))
        return;
    gear_reset_tried_.insert(guid_low);   // not marked, or already tried since the server started (a piece that cannot be worn is not retried every tick)
    bool complete = true;
    if (bot->GetLevel() < ::Playerbot::Gear::kWorldGearMinLevel)
    {
        uint32 const removed = ::Playerbot::API(bot).reset_to_starter_gear(complete);
        TC_LOG_INFO("playerbot.v2", "[world] {} (L{}) starter outfit {}: {} equipped items replaced or removed", bot->GetName(), uint32(bot->GetLevel()),
                    complete ? "on" : "NOT complete, kept for the next login", removed);
    }
    if (!complete)
        return;   // the mark stays: the next login tries again, and the old pieces were only taken off where a starter piece went on
    gear_reset_pending_.erase(guid_low);
    CharacterDatabase.PExecute("UPDATE playerbot_v2_character SET world_gear_pending = 0 WHERE character_guid_low = {}", guid_low);
}

void WorldBotSpawner::DrivePending(uint32 now_ms, std::vector<Anchor> const&)
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        uint64 const guid_low = it->first;
        Pending& pd = it->second;
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(guid_low);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        // Once the pending list is read, a bot that is up and set up is looked at for the one-time outfit reset before anything else moves it.
        if (gear_reset_loaded_ && bot && bot->IsInWorld() && !bot->IsBeingTeleported() && owner_.SetupDone(guid_low))
            ResetStarterGearOnce(bot);

        bool drop = getMSTimeDiff(pd.queued_ms, now_ms) >= kPendingTimeoutMs;
        if (pd.global)
        {
            // World-wide filler: nothing to place, the setup pipeline already sends it to a leveling town.
            if (pd.wake && !pd.unstacked && !drop && bot && bot->IsInWorld() && !bot->IsBeingTeleported() && owner_.SetupDone(guid_low))
            {
                StepOffStack(bot);
                pd.unstacked = true;
            }
            if (drop || (bot && owner_.SetupDone(guid_low) && (!pd.wake || pd.unstacked)))
            {
                if (!drop) ++n_global_settled_;
                it = pending_.erase(it);
            }
            else
                ++it;
            continue;
        }
        if (!drop && bot && bot->IsInWorld() && !bot->IsBeingTeleported() && owner_.SetupDone(guid_low))
        {
            // A bot that already queued for (or entered) a battleground, or is in an instance or a group, is not ours
            // to move any more: leave it alone, the BG / group logic owns it.
            if (bot->InBattleground() || bot->InBattlegroundQueue() || bot->GetGroup()
                || (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsBattlegroundOrArena())))
            {
                TC_LOG_INFO("playerbot.v2", "[world] abandon {} ({}): bg={} queued={} group={} instance={}", bot->GetName(), guid_low,
                            bot->InBattleground(), bot->InBattlegroundQueue(), bot->GetGroup() != nullptr,
                            bot->GetMap() && bot->GetMap()->IsDungeon());
                it = pending_.erase(it);
                ++n_abandoned_;
                continue;
            }
            Player* anchor = ObjectAccessor::FindConnectedPlayer(pd.anchor);
            if (!anchor || !anchor->IsInWorld() || !anchor->GetMap() || anchor->GetMap()->IsDungeon()
                || anchor->GetMap()->IsBattlegroundOrArena())
            {
                // The player left; the despawn pass will log the bot out once its grace is up.
                TC_LOG_INFO("playerbot.v2", "[world] abandon {} ({}): anchor left the open world", bot->GetName(), guid_low);
                placed_[guid_low] = Placed{ now_ms };
                it = pending_.erase(it);
                ++n_abandoned_;
                continue;
            }
            if (!bot->IsInCombat() && bot->IsAlive() && PlaceNear(bot, anchor))
            {
                // The teleport lands after this call, so the bot's own position and zone are still where it logged in: the line names the
                // destination (and the anchor's zone), and "was" is the position it came from.
                TC_LOG_INFO("playerbot.v2", "[world] placed {} ({}) L{} to ({:.0f},{:.0f},{:.0f}) in zone {}, {:.0f} y from anchor, after {} ms | was zone {} ({:.0f},{:.0f},{:.0f}) map {}",
                            bot->GetName(), guid_low, bot->GetLevel(), last_place_x_, last_place_y_, last_place_z_, anchor->GetZoneId(), last_place_dist_,
                            getMSTimeDiff(pd.queued_ms, now_ms), bot->GetZoneId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
                placed_[guid_low] = Placed{ now_ms };
                ++n_placed_;
                // A town (capital: no zone level range): most bots stay there and do town things, so it looks busy.
                if (::Playerbot::V2::World::IsVanillaCapitalZone(anchor->GetZoneId()) && urand(0, 99) < cfg_.town_pct)
                    if (BotAI* ai = Services::Registry().ai(guid_low))
                        ai->force_activity_mode(BotAI::ActivityMode::Professioning, GameTime::GetGameTimeMS() + urand(45u, 90u) * 60u * 1000u);
                it = pending_.erase(it);
                continue;
            }
            // In combat / dead / no ground found: retry next tick until the timeout.
        }
        if (drop)
        {
            TC_LOG_INFO("playerbot.v2", "[world] abandon {}: login or setup did not finish in time (online={} setup_done={})", guid_low,
                        bot != nullptr, owner_.SetupDone(guid_low));
            placed_[guid_low] = Placed{ now_ms };   // despawn pass cleans it up if it did come online
            it = pending_.erase(it);
            ++n_abandoned_;
            continue;
        }
        ++it;
    }
}

void WorldBotSpawner::DriveDespawn(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    // Forget placement records of bots that are gone.
    for (auto it = placed_.begin(); it != placed_.end();)
    {
        if (!ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(it->first)))
            it = placed_.erase(it);
        else
            ++it;
    }

    struct Victim { ObjectGuid guid; float score; bool orphan; bool trail; };   // score: larger = kicked first; orphan: beyond DespawnRadius
    std::vector<Victim> victims;
    uint32 ambient = 0;
    uint32 far_bots = 0;
    uint32 all_bots = 0;
    uint32 owned_bots = 0;   // companions and a player's own bots: never part of the world cap
    uint32 tied_bots = 0;    // ambient bots in a group, instance or battleground: not part of the world cap either
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p || !p->IsInWorld()) return;
        ++all_bots;
        if (!owner_.IsAmbientBot(id, p)) { ++owned_bots; return; }
        // Bots queued for / inside a battleground, in a group or in an instance are not part of the open-world population.
        if (p->InBattleground() || p->InBattlegroundQueue() || p->GetGroup()
            || (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())))
        {
            ++tied_bots;
            return;
        }
        ++ambient;
        if (pending_.contains(uint64(id)) || seeding_.contains(uint64(id)) || leaving_.contains(uint64(id))) return;
        if (::Playerbot::V2::Travel::SoakBoard::Instance().Has(uint64(id))) return;   // a soak runner is far from every anchor on purpose

        // Distance to the nearest anchor on the bot's own map.
        float nearest = 1e9f;
        bool misfit = false;   // in a start zone it does not fit (RaceFitsStartZone), near a player standing in that zone
        uint32 const bot_zone = p->GetZoneId();
        bool const unfit_here = !p->IsInFlight()
            && !::Playerbot::V2::World::RaceFitsStartZone(p->GetRace(), p->GetLevel(), bot_zone);
        for (Anchor const& a : anchors)
            if (a.map == p->GetMapId())
            {
                float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), a.x, a.y);
                nearest = std::min(nearest, d);
                if (unfit_here && a.zone == bot_zone && d <= cfg_.activity_radius)
                    misfit = true;
            }
        if (nearest > cfg_.activity_radius) ++far_bots;
        // Left far behind a rider (Moving.DespawnBehindYards) and in nobody's bubble: it goes as an orphan does, so the bots and the RAM
        // go to the road ahead of him instead of waiting for him to be DespawnRadius away (Fleet/MovingBubble.h).
        bool trail = false;
        if (cfg_.moving_enable && cfg_.moving_despawn_behind > 0.0f)
        {
            bool counted = false;
            for (Anchor const& a : anchors)
                if (a.map == p->GetMapId())
                {
                    if (InBubbleOf(a, p->GetPositionX(), p->GetPositionY())) { counted = true; break; }
                    if (LeftBehind(a.x, a.y, a.motion, p->GetPositionX(), p->GetPositionY(), cfg_.moving_despawn_behind)) trail = true;
                }
            trail = trail && !counted;
        }
        // Plain bot numbers: a zone nobody has been in for LeaveZoneSec loses its bots, unless one stands inside a player's spawn ring
        // (a bot just over a zone line next to him stays; Karrz 11-01: "why would you hold 50 bots still active there?").
        // Inside a player's bubble (ActivityRadius) the bot counts for him, so it stays even over a zone line: logging it out there only
        // had the spawner put a new one in its place (a bot grinding 360 y away in the next zone went, and came back as another one).
        bool emptied = false;
        if (cfg_.plain && cfg_.leave_zone_ms && !p->IsInFlight() && nearest > cfg_.activity_radius)
            emptied = !place_seen_ms_.contains(::Playerbot::V2::Fleet::PlaceKey(p->GetMapId(), bot_zone));
        // A misfit goes as an orphan does: a level 25 human priest in Deathknell takes the new player's mobs (PC 26 Oct).
        bool const orphan = nearest > cfg_.despawn_radius || misfit || trail || emptied;
        // A bot behind the player (past ActivityRadius, inside DespawnRadius) can be recycled early when the world cap is full and a
        // player's bubble still lacks bots; otherwise only orphans go.
        if (!orphan && nearest <= cfg_.activity_radius)
            return;

        // Not while it is doing something that logging out would break.
        if (p->IsInCombat() || p->GetGroup() || p->IsBeingTeleported()) return;
        if (Map* m = p->GetMap(); m && (m->IsDungeon() || m->IsBattlegroundOrArena())) return;
        if (p->InBattlegroundQueue() || p->InBattleground()) return;
        if (owner_.IsKickProtected(id, now_ms)) return;
        if (auto pit = placed_.find(uint64(id)); !emptied && pit != placed_.end()   // an emptied zone does not wait for the placing grace
            && getMSTimeDiff(pit->second.placed_ms, now_ms) < cfg_.placed_grace_ms)
            return;
        // Bots of the world-wide filler are only recycled once their setup is done (not mid-pipeline).
        if (cfg_.global_fill && !owner_.SetupDone(uint64(id))) return;
        victims.push_back({ g, misfit ? 1e9f : nearest, orphan, trail });   // a misfit first: it is right next to the player
    });
    ambient_online_ = ambient;
    global_online_ = far_bots;
    all_online_ = all_bots;
    owned_online_ = owned_bots;
    tied_online_ = tied_bots;

    // The bubbles' shortfall: how many bots the players' targets still lack.
    uint32 deficit = 0;
    for (Anchor const& a : anchors)
    {
        uint32 pend = 0;
        for (auto const& [gl, pd] : pending_)
            if (pd.anchor == a.guid) ++pend;
        uint32 const have = CountAmbientNear(a) + pend;
        uint32 const tgt = TargetFor(a);
        if (have < tgt) deficit += tgt - have;
    }
    uint32 const used = ambient + uint32(pending_.size());
    uint32 const free_budget = cfg_.max_bots > used ? cfg_.max_bots - used : 0;
    // With the world-wide filler a far bot is kept while there is budget; it is only recycled (farthest first) to
    // make room for the bubble around a real player that still has a deficit.
    uint32 limit = cfg_.despawn_per_tick * (int32(swap_burst_until_ms_ - now_ms) > 0 ? kSwapBurstFactor : 1u);
    if (cfg_.global_fill && !anchors.empty())
        limit = deficit > free_budget ? std::min(limit, deficit - free_budget) : 0;
    // At the world cap with a bubble still short: the bots farthest behind the player are recycled even inside DespawnRadius, so the
    // zone he walks into fills at once instead of waiting for the ones behind him to age out.
    bool const at_cap = used >= cfg_.max_bots && deficit > 0;
    if (at_cap)
        limit = std::min(std::max(limit, 1u), deficit);

    // Over the global cap with every bot still near someone: nothing to do here, DriveSpawn simply stops adding.
    std::sort(victims.begin(), victims.end(), [](Victim const& a, Victim const& b) { return a.score > b.score; });
    uint32 n = 0;
    for (Victim const& v : victims)
    {
        if (n >= limit) break;
        if (!v.orphan && !at_cap) continue;
        Player* vp = ObjectAccessor::FindConnectedPlayer(v.guid);
        if (!vp) continue;
        bool const was_awake = vp->IsInWorld();
        if (RetireBot(vp, now_ms, anchors))
        {
            ++n;
            if (!was_awake || !leaving_.contains(v.guid.GetCounter()))
            {
                ++n_despawned_;
                placed_.erase(v.guid.GetCounter());
            }
            if (!v.orphan) ++n_evicted_;
            if (v.trail) ++n_left_behind_;
        }
    }
}

bool WorldBotSpawner::RetireBot(Player* bot, uint32 now_ms, std::vector<Anchor> const& anchors)
{
    constexpr float kSeenYards = 100.0f;      // a player this near, with a line of sight, sees the bot
    constexpr float kAwayYards = 120.0f;      // where a bot with no better way out walks to
    constexpr float kInnYards = 250.0f;
    ObjectGuid const g = bot->GetGUID();
    if (leaving_.contains(g.GetCounter()))
        return false;   // already on its way out
    auto seen_by = [&](Anchor const*& nearest_out)
    {
        bool seen = false;
        float best = 1e9f;
        for (Anchor const& a : anchors)
        {
            if (a.map != bot->GetMapId()) continue;
            float const d = Dist2D(bot->GetPositionX(), bot->GetPositionY(), a.x, a.y);
            if (d < best) { best = d; nearest_out = &a; }
            if (d > kSeenYards || seen) continue;
            Player* ap = ObjectAccessor::FindConnectedPlayer(a.guid);
            if (ap && ap->IsInWorld() && ap->IsWithinLOS(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ() + 1.5f))
                seen = true;
        }
        return seen;
    };
    Anchor const* near_a = nullptr;
    if (!cfg_.natural_moves || !seen_by(near_a) || !near_a)
        return Services::SessionMgr().LogoutBot(g);

    // In sight: walk out of it, then log out once unseen. In a capital to the city gate: the nearest inn took every retiring bot of the
    // city, and with the player in the inn none of them ever got out of his sight (a crowd in the Gilded Rose, live 273). Elsewhere to a
    // place away from the player that he cannot see, else the nearest inn, else straight away.
    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    bool have = false;
    bool const capital = ::Playerbot::V2::World::IsVanillaCapitalZone(bot->GetZoneId());
    {
        ::Playerbot::V2::GateDuels::GateSpot gate;
        uint32 city = 0;
        if (capital && ::Playerbot::V2::GateDuels::Board::Get().GateNear(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), 450.0f,
                                                                           bot->GetTeam() == ALLIANCE, gate, city))
        {
            tx = gate.x; ty = gate.y; tz = gate.z;
            have = true;
        }
    }
    if (!have)
        if (Player* ap = ObjectAccessor::FindConnectedPlayer(near_a->guid); ap && ap->IsInWorld() && ap->GetMapId() == bot->GetMapId())
        {
            float const away = std::atan2(bot->GetPositionY() - near_a->y, bot->GetPositionX() - near_a->x);
            for (int k = 0; k < 8 && !have; ++k)
            {
                float const ang = away + (float(k % 4) - 1.5f) * 0.45f;
                float const d = k < 4 ? 70.0f : 110.0f;
                float const wx = bot->GetPositionX() + std::cos(ang) * d, wy = bot->GetPositionY() + std::sin(ang) * d;
                float const wz = bot->GetMap()->GetHeight(bot->GetPhaseShift(), wx, wy, bot->GetPositionZ() + 20.0f, true, 60.0f);
                Position nav;
                if (wz <= INVALID_HEIGHT || !::Playerbot::BotMovement::NearestNavPoint(bot, wx, wy, wz, 10.0f, 8.0f, nav)) continue;
                if (InMagma(bot->GetMap(), bot->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 0.05f)) continue;
                if (ap->IsWithinLOS(nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 1.5f)) continue;
                tx = nav.GetPositionX(); ty = nav.GetPositionY(); tz = nav.GetPositionZ();
                have = true;
            }
        }
    if (!have && !capital)
        if (auto const* inn = ::Playerbot::V2::Travel::TownNpcIndex::Instance().Nearest(
                bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), UNIT_NPC_FLAG_INNKEEPER,
                bot->GetTeam() == ALLIANCE, kInnYards))
        {
            tx = inn->x; ty = inn->y; tz = inn->z;
            have = true;
        }
    if (!have)
    {
        float dx = bot->GetPositionX() - near_a->x, dy = bot->GetPositionY() - near_a->y;
        float const len = std::hypot(dx, dy);
        if (len < 1.0f) { dx = 1.0f; dy = 0.0f; } else { dx /= len; dy /= len; }
        Position nav;
        float const wx = bot->GetPositionX() + dx * kAwayYards, wy = bot->GetPositionY() + dy * kAwayYards;
        float const wz = bot->GetMap()->GetHeight(bot->GetPhaseShift(), wx, wy, bot->GetPositionZ() + 20.0f, true, 60.0f);
        if (wz > INVALID_HEIGHT && ::Playerbot::BotMovement::NearestNavPoint(bot, wx, wy, wz, 15.0f, 8.0f, nav)
            && !InMagma(bot->GetMap(), bot->GetPhaseShift(), nav.GetPositionX(), nav.GetPositionY(), nav.GetPositionZ() + 0.05f))
        {
            tx = nav.GetPositionX(); ty = nav.GetPositionY(); tz = nav.GetPositionZ();
            have = true;
        }
    }
    BotAI* ai = Services::Registry().ai(g.GetCounter());
    if (!have || !ai)
        return Services::SessionMgr().LogoutBot(g);
    ai->walk_order().Set(::Playerbot::V2::WalkOrder::Leave, tx, ty, tz);
    leaving_[g.GetCounter()] = now_ms;
    ++n_left_walking_;
    return true;
}

// Bots that were sent walking away log out once no player can see them, or after a minute (a bot that cannot get out of sight must
// not stay awake for good).
void WorldBotSpawner::DriveLeaving(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    constexpr uint32 kLeaveTimeoutMs = 60000u;
    constexpr float kSeenYards = 100.0f;
    for (auto it = leaving_.begin(); it != leaving_.end();)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(it->first);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot || !bot->IsInWorld())
        {
            it = leaving_.erase(it);
            continue;
        }
        bool seen = false;
        for (Anchor const& a : anchors)
        {
            if (a.map != bot->GetMapId() || Dist2D(bot->GetPositionX(), bot->GetPositionY(), a.x, a.y) > kSeenYards) continue;
            Player* ap = ObjectAccessor::FindConnectedPlayer(a.guid);
            if (ap && ap->IsInWorld() && ap->IsWithinLOS(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ() + 1.5f))
            {
                seen = true;
                break;
            }
        }
        bool const in_trouble = bot->IsInCombat() || bot->GetGroup() || bot->InBattleground() || bot->InBattlegroundQueue();
        if (in_trouble)
        {
            if (BotAI* ai = Services::Registry().ai(it->first)) ai->walk_order().Clear();
            it = leaving_.erase(it);   // not leaving after all: the next despawn pass decides again
            continue;
        }
        if (!seen || getMSTimeDiff(it->second, now_ms) >= kLeaveTimeoutMs)
        {
            if (BotAI* ai = Services::Registry().ai(it->first)) ai->walk_order().Clear();
            if (Services::SessionMgr().LogoutBot(g))
            {
                ++n_despawned_;
                placed_.erase(it->first);
            }
            it = leaving_.erase(it);
            continue;
        }
        ++it;
    }
}

void WorldBotSpawner::WindowFor(Anchor const& a, uint8& lo, uint8& hi) const
{
    // Zephras Isle: its own levels, below MinLevel. The island's bots are Skyborne questers; they are not lifted to 10, which
    // is where a bot leaves the island (DriveGraduation).
    if (Skyborne::IsOnIsle(a.map))
    {
        Skyborne::Window const w = Skyborne::IsleWindow();
        lo = w.lo;
        hi = w.hi;
        return;
    }
    // The player's own level inside the zone's range (a level 1 in Northshire meets level 1-3 bots, not level 10 ones), whatever the
    // zone; MinLevel keeps the wide spread of a capital and the world-wide filler up, it no longer lifts a starter zone's bots.
    uint8 zlo = 0, zhi = 0;
    bool const zone_known = ::Playerbot::V2::World::VanillaZoneLevelRange(a.zone, zlo, zhi);
    ::Playerbot::V2::Fleet::LevelWindow const w = ::Playerbot::V2::Fleet::AnchorLevelWindow(
        int(a.level), zone_known, int(zlo), int(zhi), int(cfg_.min_level), int(cfg_.max_level));
    lo = uint8(w.lo);
    hi = uint8(w.hi);
    // A start zone's residents stay at the start levels, whoever stands there: a level 60 in Tirisfal asked for level 11 bots (the zone's
    // range plus one), and RaceFitsStartZone refuses those there.
    if (::Playerbot::V2::World::IsStartZone(a.zone) && hi > ::Playerbot::V2::World::kStartZoneMaxLevel)
    {
        hi = uint8(::Playerbot::V2::World::kStartZoneMaxLevel);
        lo = std::min(lo, hi);
    }
}

void WorldBotSpawner::DriveSpawn(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    if (anchors.empty() || query_inflight_)
        return;
    if (pool_count_ < 0)
        return;   // the pool size is not known yet
    if (ambient_online_ + pending_.size() >= cfg_.max_bots)
        return;
    if (cfg_.rss_budget_mb && rss_kb_ / 1024 > cfg_.rss_budget_mb * 115 / 100)
        return;   // far over the budget: even the bubble waits

    // Pick the next anchor, round robin, that still has a deficit.
    for (size_t k = 0; k < anchors.size(); ++k)
    {
        Anchor const& a = anchors[(next_anchor_ + k) % anchors.size()];
        uint32 pending_for_anchor = 0;
        for (auto const& [guid, pd] : pending_)
            if (pd.anchor == a.guid) ++pending_for_anchor;
        uint32 const have = CountAmbientNear(a) + pending_for_anchor;
        uint32 const target = TargetFor(a);
        if (have >= target)
        {
            last_have_ = have; last_target_ = target;
            ++n_at_target_;
            continue;
        }

        next_anchor_ = (next_anchor_ + k + 1) % anchors.size();

        uint32 const burst = int32(swap_burst_until_ms_ - now_ms) > 0 ? kSwapBurstFactor : 1u;
        uint32 want = std::min({ target - have, SpawnRateFor(a) * burst,
                                 cfg_.max_bots - uint32(ambient_online_ + pending_.size()) });
        if (!want) return;

        // Persistent residents first: pool bots that sleep (offline, saved) around this player wake up where they stand. A
        // pass that finds nobody asleep pauses for 15 s and the ring path below places new residents instead; they stay
        // wherever they are when logged out, so the next visit finds them.
        if (cfg_.wake_radius > 0.0f)
        {
            auto const dry = wake_dry_until_.find(a.guid.GetCounter());
            if (dry == wake_dry_until_.end() || int32(dry->second - now_ms) <= 0)
            {
                DriveWake(now_ms, a, want);
                return;
            }
        }

        // Random order, not oldest-seen first: the oldest rows are the same ones every pass, and when they are all of the other faction
        // (or never match the slots) the pass matched nothing and the anchor's zone stayed empty (Duskwood 0/8 each pass in a soak run).
        // Level range that suits the anchor's zone; capitals and the like have none, so spread around the player.
        uint8 lo = 0, hi = 0;
        WindowFor(a, lo, hi);

        std::vector<bool>  slots;        // true = alliance
        std::vector<uint8> slot_levels;
        for (uint32 i = 0; i < want; ++i)
        {
            slots.push_back(BotIsAllianceInZone(a.zone, a.alliance, urand(0, 99), FriendlyPctFor(cfg_.mix, a.zone)));
            slot_levels.push_back(uint8(urand(lo, hi)));
        }

        query_inflight_ = true;
        query_started_ms_ = now_ms;
        queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
            "SELECT pv.character_guid_low, c.level, c.race, c.map, c.position_x, c.position_y, c.position_z FROM playerbot_v2_character pv "
            "JOIN characters c ON c.guid = pv.character_guid_low "
            "WHERE c.level BETWEEN {} AND {} AND c.online = 0 AND pv.distribution_level > 0 {}"
            "ORDER BY RAND() LIMIT {}",
            uint32(lo), uint32(hi), SkyborneRaceClause(Skyborne::IsOnIsle(a.map)), std::max<uint32>(want * 20u, 100u)).c_str())
            .WithCallback([this, a, slots, slot_levels](QueryResult res)
        {
            query_inflight_ = false;
            FinishSpawn(a, slots, slot_levels, std::move(res));
        }));
        return;
    }

    DriveGlobalSpawn(now_ms, anchors);
}

// Where more world bots stand within AreaRadius of one another than the area cap allows (outdoors fewer than in a capital), the
// extras are placed again next to the nearest player, which refuses spots that are full. A few per pass, never a bot in combat, in a
// group, an instance or a battleground, or still being set up. Logged once per move.
void WorldBotSpawner::DriveCrowd(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    constexpr uint32 kPassMs = 10000u;
    constexpr size_t kMovesPerPass = 3u;
    if ((!cfg_.area_cap_outdoors && !cfg_.area_cap_town) || anchors.empty())
        return;
    if (last_crowd_ms_ && getMSTimeDiff(last_crowd_ms_, now_ms) < kPassMs)
        return;
    last_crowd_ms_ = now_ms;

    std::unordered_map<uint32, std::vector<::Playerbot::V2::Fleet::CrowdPoint>> by_map;
    std::unordered_map<uint64, Player*> movable;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->GetMap() || !owner_.IsAmbientBot(id, p)) return;
        if (p->InBattleground() || p->InBattlegroundQueue() || p->GetGroup() || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())
            return;
        if (::Playerbot::V2::Travel::SoakBoard::Instance().Has(uint64(id))) return;   // a soak runner is not re-placed next to a player
        if (::Playerbot::V2::Gathering::Board::Get().Has(uint64(id))) return;          // nor a bot at a gathering outside a gate
        by_map[p->GetMapId()].push_back({ uint64(id), p->GetPositionX(), p->GetPositionY(), AreaCapFor(p->GetZoneId()) });
        if (p->IsAlive() && !p->IsInCombat() && !p->IsBeingTeleported() && !p->IsInFlight() && !p->GetTransport()
            && owner_.SetupDone(uint64(id)) && !pending_.contains(uint64(id)) && !seeding_.contains(uint64(id)))
            movable[uint64(id)] = p;
    });
    size_t moved = 0;
    for (auto const& [map_id, pts] : by_map)
    {
        for (uint64 const id : ::Playerbot::V2::Fleet::ExcessBots(pts, cfg_.area_radius, 64u))
        {
            if (moved >= kMovesPerPass) return;
            auto mv = movable.find(id);
            if (mv == movable.end()) continue;
            Player* p = mv->second;
            Anchor const* near_a = nullptr;
            float near_d = 1e9f;
            for (Anchor const& a : anchors)
            {
                if (a.map != map_id) continue;
                float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), a.x, a.y);
                if (d < near_d) { near_d = d; near_a = &a; }
            }
            if (!near_a || near_d > cfg_.despawn_radius) continue;   // nobody near: the despawn pass logs it out
            Player* ap = ObjectAccessor::FindConnectedPlayer(near_a->guid);
            if (!ap || !ap->IsInWorld()) continue;
            float const fx = p->GetPositionX(), fy = p->GetPositionY();
            if (!PlaceNear(p, ap)) continue;
            placed_[id] = Placed{ now_ms };
            ++moved;
            ++n_crowd_moved_;
            TC_LOG_INFO("playerbot.v2", "[world] crowd: {} ({}) was one of more than the cap within {:.0f} y at ({:.0f},{:.0f}) zone {}, placed again next to {}",
                        p->GetName(), id, cfg_.area_radius, fx, fy, p->GetZoneId(), ap->GetName());
        }
    }
}

// ---- Gate duels (Fleet/GateDuels.h) ------------------------------------------------------------------------------------------------
// The first walkable point outside the city on the road to the next town, found on the real navmesh by chaining path calls the way
// ReachableChain does, and taking GateDuels::GateFromPath of the points. `src` is a bot inside the city.
// The game refuses the Duel spell where the area has no AllowDueling flag (Spell::EffectDuel: the cast goes off, nothing happens, no flag).
// 0 = allowed; otherwise the area id the refusal is about.
static uint32 DuelAreaRefusal(Map* map, PhaseShift const& phase, float x, float y, float z)
{
    uint32 const area = map->GetAreaId(phase, x, y, z);
    AreaTableEntry const* e = sAreaTableStore.LookupEntry(area);
    return e && !e->GetFlags().HasFlag(AreaFlags::AllowDueling) ? area : 0u;
}

static bool FindGateSpot(Player* src, GateDuels::City const& city, float& gx, float& gy, float& gz, float out_yards = GateDuels::kOutYards)
{
    Map* map = src->GetMap();
    if (!map) return false;
    std::vector<GateDuels::PathPt> pts;
    auto const inside = [&](G3D::Vector3 const& v) { return map->GetZoneId(src->GetPhaseShift(), v.x, v.y, v.z) == city.zone; };
    float sx = src->GetPositionX(), sy = src->GetPositionY(), sz = src->GetPositionZ();
    pts.push_back({ sx, sy, sz, map->GetZoneId(src->GetPhaseShift(), sx, sy, sz) == city.zone });
    for (uint32 hop = 0; hop < 6u; ++hop)
    {
        PathGenerator pg(src);
        ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, sx, sy, sz, city.tx, city.ty, city.tz);
        PathType const t = pg.GetPathType();
        if (t & PATHFIND_NOPATH)
            return false;
        Movement::PointsArray const& path = pg.GetPath();
        for (std::size_t i = 1; i < path.size(); ++i)
            pts.push_back({ path[i].x, path[i].y, path[i].z, inside(path[i]) });
        if (GateDuels::GateFromPath(pts, out_yards, gx, gy, gz))
            return true;
        if (!(t & PATHFIND_INCOMPLETE) || path.size() < 2)
            return false;
        G3D::Vector3 const& end = path.back();
        if (std::hypot(end.x - sx, end.y - sy) < 3.0f)
            return false;
        sx = end.x; sy = end.y; sz = end.z;
    }
    return false;
}

// Pairs of world bots duel outside the gate of a capital while a real player of that faction is within Radius of it, the classic sight.
// The driver picks the pairs and the spots, the idle rules (idle:gate_duel) walk the bots there, the challenger casts Duel (a real
// request: the game's duel flag, countdown and rules apply), the duel runs the usual way. A pair that has fought sits out for a while,
// so the pairs rotate; when no player is near the pairs are released.
void WorldBotSpawner::DriveGateDuels(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    using namespace ::Playerbot::V2::GateDuels;
    constexpr uint32 kPassMs = 4000u;
    Board& board = Board::Get();
    auto const& gcfg = Services::Config();
    auto release = [&](GateSlot const& s)
    {
        board.Clear(s.a); board.Clear(s.b); board.Clear(s.watch[0]); board.Clear(s.watch[1]);
    };
    if (!gcfg.gate_duels_enable() || anchors.empty())
    {
        if (!gate_slots_.empty())
        {
            for (GateSlot const& s : gate_slots_) release(s);
            gate_slots_.clear();
        }
        return;
    }
    if (last_gate_ms_ && getMSTimeDiff(last_gate_ms_, now_ms) < kPassMs)
        return;
    last_gate_ms_ = now_ms;

    float const radius = gcfg.gate_duels_radius();
    constexpr float kCandidateYards = 150.0f;   // only bots this near the gate are drawn into a pair: a short walk, not a trek across the city
    uint32 const pairs_cap = gcfg.gate_duels_pairs();
    bool near_city[kCityCount] = {};
    bool any_near = false;
    for (std::size_t ci = 0; ci < kCityCount; ++ci)
        for (Anchor const& a : anchors)
            if (a.map == kCities[ci].map && a.alliance == kCities[ci].alliance && Dist2D(a.x, a.y, kCities[ci].cx, kCities[ci].cy) <= 400.0f)
            { near_city[ci] = true; any_near = true; break; }
    if (!any_near)
    {
        for (GateSlot const& s : gate_slots_) release(s);
        gate_slots_.clear();
        return;
    }

    // The ambient bots that could take part (alive, free, on the map of a city a player is near).
    std::unordered_map<uint64, Player*> bots;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !p->GetMap() || p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport()) return;
        if (!owner_.IsAmbientBot(id, p) || p->GetGroup() || p->InBattleground() || p->InBattlegroundQueue()
            || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena()) return;
        if (!owner_.SetupDone(uint64(id)) || pending_.contains(uint64(id)) || seeding_.contains(uint64(id))) return;
        for (std::size_t ci = 0; ci < kCityCount; ++ci)
            if (near_city[ci] && kCities[ci].map == p->GetMapId()) { bots.emplace(uint64(id), p); return; }
    });
    if (gate_rest_.size() > 512u)
        for (auto it = gate_rest_.begin(); it != gate_rest_.end();)
            it = (now_ms > it->second) ? gate_rest_.erase(it) : std::next(it);
    auto const rest = [&](uint64 id, uint32 ms) { gate_rest_[id] = now_ms + ms; };
    auto const find_bot = [&](uint64 id) -> Player*
    {
        auto const it = bots.find(id);
        return it == bots.end() ? nullptr : it->second;
    };

    for (std::size_t ci = 0; ci < kCityCount; ++ci)
    {
        City const& city = kCities[ci];
        GateState& gs = gate_state_[ci];
        auto const release_city = [&]()
        {
            for (auto it = gate_slots_.begin(); it != gate_slots_.end();)
                if (it->city == ci) { release(*it); it = gate_slots_.erase(it); } else ++it;
        };
        if (!near_city[ci]) { release_city(); continue; }

        if (gs.state != 1u && (!gs.retry_ms || now_ms >= gs.retry_ms))
        {
            Player* src = nullptr;
            for (auto const& [id, p] : bots)
                if (p->GetMapId() == city.map && p->GetZoneId() == city.zone && !p->IsInCombat() && (p->GetTeam() == ALLIANCE) == city.alliance) { src = p; break; }
            if (!src)
            {
                gs.retry_ms = now_ms + 10000u;
                if (getMSTimeDiff(gate_why_ms_[ci], now_ms) >= 60000u)
                {
                    gate_why_ms_[ci] = now_ms;
                    TC_LOG_INFO("playerbot.v2", "[gate_duel] {} a player is near but no free bot of his side is up in the city ({} bots counted on the map)", city.name, uint32(bots.size()));
                }
                continue;
            }
            float gx = 0, gy = 0, gz = 0;
            bool found = FindGateSpot(src, city, gx, gy, gz);
            // Walk the spot out along the road until the game allows a duel there (the Gates of Ironforge may be a no-duel area).
            for (float yards : { 40.0f, 70.0f, 110.0f, 160.0f })
            {
                if (!found) break;
                uint32 const refused = DuelAreaRefusal(src->GetMap(), src->GetPhaseShift(), gx, gy, gz);
                if (!refused) break;
                TC_LOG_INFO("playerbot.v2", "[gate_duel] {} gate at ({:.0f},{:.0f}) is in area {} where dueling is not allowed: trying {:.0f} yd out", city.name, gx, gy, refused, yards);
                found = FindGateSpot(src, city, gx, gy, gz, yards);
            }
            if (found && DuelAreaRefusal(src->GetMap(), src->GetPhaseShift(), gx, gy, gz))
                found = false;
            if (found)
            {
                gs.state = 1u; gs.x = gx; gs.y = gy; gs.z = gz;
                board.SetGate(uint32(ci), GateSpot{ true, city.map, gx, gy, gz, city.alliance });
                TC_LOG_INFO("playerbot.v2", "[gate_duel] {} gate found at ({:.0f},{:.0f},{:.0f}) on the road out of the city", city.name, gx, gy, gz);
            }
            else
            {
                gs.state = 2u; gs.retry_ms = now_ms + 300000u;
                TC_LOG_INFO("playerbot.v2", "[gate_duel] {} no gate found from {} toward ({:.0f},{:.0f}): no duels there, tried again in 5 min", city.name, src->GetName(), city.tx, city.ty);
            }
        }
        if (gs.state != 1u) { release_city(); continue; }

        bool player_near = false;
        for (Anchor const& a : anchors)
            if (a.map == city.map && a.alliance == city.alliance && Dist2D(a.x, a.y, gs.x, gs.y) <= radius) { player_near = true; break; }
        if (!player_near)
        {
            if (getMSTimeDiff(gate_why_ms_[ci], now_ms) >= 60000u)
            {
                gate_why_ms_[ci] = now_ms;
                float nearest = 1e9f;
                for (Anchor const& a : anchors)
                    if (a.map == city.map && a.alliance == city.alliance) nearest = std::min(nearest, Dist2D(a.x, a.y, gs.x, gs.y));
                TC_LOG_INFO("playerbot.v2", "[gate_duel] {} the player near the city is {:.0f} yd from the gate at ({:.0f},{:.0f}), duels start within {:.0f}", city.name, nearest, gs.x, gs.y, radius);
            }
            release_city();
            continue;
        }

        // The pairs in progress.
        uint32 used_slots = 0;
        for (auto it = gate_slots_.begin(); it != gate_slots_.end();)
        {
            GateSlot& s = *it;
            if (s.city != ci) { ++it; continue; }
            Player* pa = find_bot(s.a);
            Player* pb = find_bot(s.b);
            char const* end_why = nullptr;
            uint32 rest_ms = kRestAfterMs;
            if (!pa || !pb || pa->GetMapId() != city.map || pb->GetMapId() != city.map)
            { end_why = "a duellist is gone"; rest_ms = 60000u; }
            else
            {
                bool const dueling = pa->duel && pb->duel && pa->duel->Opponent == pb && pa->duel->State == DUEL_STATE_IN_PROGRESS;
                if (dueling && !s.started)
                {
                    s.started = true; s.started_ms = now_ms;
                    ::Playerbot::V2::TownLife::Counters().gate_duels.fetch_add(1, std::memory_order_relaxed);
                    TC_LOG_INFO("playerbot.v2", "[gate_duel] {} vs {} at {}: duel started", pa->GetName(), pb->GetName(), city.name);
                }
                else if (s.started && !dueling)
                { end_why = "duel over"; }
                else if (!s.started && getMSTimeDiff(s.since_ms, now_ms) > kStartDeadlineMs)
                {
                    end_why = "no duel started in time"; rest_ms = kRestFailedMs;
                    Assignment aa, ab;
                    board.Find(s.a, aa); board.Find(s.b, ab);
                    TC_LOG_INFO("playerbot.v2", "[gate_duel] {} timeout facts: {} at ({:.0f},{:.0f}) spot ({:.0f},{:.0f}) area {} moving={} | {} at ({:.0f},{:.0f}) spot ({:.0f},{:.0f}) area {} moving={} | apart {:.1f} yd | duel flags a={} b={} | levels {}/{}",
                        city.name, pa->GetName(), pa->GetPositionX(), pa->GetPositionY(), aa.x, aa.y, pa->GetAreaId(), pa->isMoving(),
                        pb->GetName(), pb->GetPositionX(), pb->GetPositionY(), ab.x, ab.y, pb->GetAreaId(), pb->isMoving(),
                        pa->GetExactDist2d(pb), pa->duel != nullptr, pb->duel != nullptr, uint32(pa->GetLevel()), uint32(pb->GetLevel()));
                }
                else if (s.started && getMSTimeDiff(s.started_ms, now_ms) > kDuelMaxMs)
                { end_why = "duel ran too long"; }
            }
            if (end_why)
            {
                TC_LOG_INFO("playerbot.v2", "[gate_duel] {} vs {} at {}: {}", pa ? pa->GetName() : std::to_string(s.a), pb ? pb->GetName() : std::to_string(s.b), city.name, end_why);
                rest(s.a, rest_ms); rest(s.b, rest_ms);
                release(s);
                it = gate_slots_.erase(it);
                continue;
            }
            used_slots |= 1u << s.slot;
            ++it;
        }

        // New pairs.
        uint32 have = 0;
        for (GateSlot const& s : gate_slots_) if (s.city == ci) ++have;
        // Orgrimmar, Ironforge and Stormwind were where the crowds were; the other three get SmallCityPairs.
        uint32 const city_cap = ::Playerbot::V2::Gathering::BigCity(ci) ? pairs_cap : std::min(pairs_cap, gcfg.gate_duels_small_city_pairs());
        if (have >= city_cap)
            continue;
        std::vector<Candidate> cands;
        for (auto const& [id, p] : bots)
        {
            if ((p->GetTeam() == ALLIANCE) != city.alliance || p->GetMapId() != city.map) continue;
            // The bots of the city walk out to the gate (idle:gate_duel); only the ones already standing at it used to count, and the crowd
            // is in the town, 150 to 400 yd away, so nobody was ever drawn.
            if (Dist2D(p->GetPositionX(), p->GetPositionY(), gs.x, gs.y) > kCandidateYards) continue;
            if (p->IsInCombat() || p->IsMounted() || p->duel || p->IsNonMeleeSpellCast(false)) continue;
            auto const r = gate_rest_.find(id);
            if (r != gate_rest_.end() && now_ms < r->second) continue;
            Assignment taken;
            if (board.Find(id, taken)) continue;
            if (::Playerbot::V2::Gathering::Board::Get().Has(id)) continue;   // sitting at a gathering
            cands.push_back({ id, uint32(p->GetLevel()), int(p->GetHealthPct()) });
        }
        if (cands.empty() && getMSTimeDiff(gate_why_ms_[ci], now_ms) >= 60000u)
        {
            gate_why_ms_[ci] = now_ms;
            TC_LOG_INFO("playerbot.v2", "[gate_duel] {} gate at ({:.0f},{:.0f}): no free bot within {:.0f} yd ({} bots counted on the map)", city.name, gs.x, gs.y, kCandidateYards, uint32(bots.size()));
        }
        if (cands.size() > 1u)
        {
            std::sort(cands.begin(), cands.end(), [&](Candidate const& l, Candidate const& r)
            {
                Player* pl = find_bot(l.id); Player* pr = find_bot(r.id);
                return pl && pr && Dist2D(pl->GetPositionX(), pl->GetPositionY(), gs.x, gs.y) < Dist2D(pr->GetPositionX(), pr->GetPositionY(), gs.x, gs.y);
            });
            if (cands.size() > 60u)
                cands.resize(60u);
        }
        std::vector<Pair> pairs = PickPairs(cands, city_cap - have, uint32(now_ms / 60000u) + uint32(ci) * 7u);
        std::unordered_set<uint64> in_pair;
        for (Pair const& pr : pairs) { in_pair.insert(pr.challenger); in_pair.insert(pr.other); }
        for (Pair const& pr : pairs)
        {
            uint32 slot = 0;
            while (slot < 3u && (used_slots & (1u << slot))) ++slot;
            if (slot >= 3u) break;
            Player* pa = find_bot(pr.challenger);
            Player* pb = find_bot(pr.other);
            if (!pa || !pb) continue;
            Layout const lay = LayoutFor(gs.x, gs.y, slot);
            auto const ground = [&](Player* p, float x, float y, float& z)
            {
                z = p->GetMap()->GetHeight(p->GetPhaseShift(), x, y, gs.z + 8.0f, true, 25.0f);
                return z > INVALID_HEIGHT && std::fabs(z - gs.z) < 12.0f;
            };
            float az = 0, bz = 0;
            uint32 used = 0;
            if (ground(pa, lay.a.x, lay.a.y, az) && ground(pb, lay.b.x, lay.b.y, bz))
            {
                uint32 refused = DuelAreaRefusal(pa->GetMap(), pa->GetPhaseShift(), lay.a.x, lay.a.y, az);
                if (!refused)
                    refused = DuelAreaRefusal(pb->GetMap(), pb->GetPhaseShift(), lay.b.x, lay.b.y, bz);
                if (refused)
                {
                    TC_LOG_INFO("playerbot.v2", "[gate_duel] {} slot {} is in area {} where dueling is not allowed: no pair there", city.name, slot, refused);
                    rest(pr.challenger, 30000u); rest(pr.other, 30000u);
                    continue;
                }
            }
            if (!ground(pa, lay.a.x, lay.a.y, az) || !ground(pb, lay.b.x, lay.b.y, bz)
                || !ReachableChain(pa, lay.a.x, lay.a.y, az, 3u, used) || !ReachableChain(pb, lay.b.x, lay.b.y, bz, 3u, used))
            {
                rest(pr.challenger, kRestFailedMs); rest(pr.other, kRestFailedMs);
                continue;
            }
            GateSlot s;
            s.city = uint32(ci); s.slot = slot; s.since_ms = now_ms; s.a = pr.challenger; s.b = pr.other;
            board.Set(pr.challenger, Assignment{ uint32(ci), Role::Challenger, pr.other, lay.a.x, lay.a.y, az });
            board.Set(pr.other, Assignment{ uint32(ci), Role::Opponent, pr.challenger, lay.b.x, lay.b.y, bz });
            // A few onlookers, any level.
            std::vector<Candidate> rest_c;
            for (Candidate const& c : cands) if (!in_pair.contains(c.id)) rest_c.push_back(c);
            std::sort(rest_c.begin(), rest_c.end(), [&](Candidate const& l, Candidate const& r) { return Mix(l.id, uint32(slot) + 91u) < Mix(r.id, uint32(slot) + 91u); });
            for (std::size_t w = 0; w < 2u && w < rest_c.size(); ++w)
            {
                Player* pw = find_bot(rest_c[w].id);
                float wz = 0;
                if (!pw || !ground(pw, lay.onlooker[w].x, lay.onlooker[w].y, wz)) continue;
                in_pair.insert(rest_c[w].id);
                s.watch[w] = rest_c[w].id;
                board.Set(rest_c[w].id, Assignment{ uint32(ci), Role::Onlooker, 0, lay.onlooker[w].x, lay.onlooker[w].y, wz });
            }
            used_slots |= 1u << slot;
            gate_slots_.push_back(s);
            TC_LOG_INFO("playerbot.v2", "[gate_duel] {} vs {} at {}", pa->GetName(), pb->GetName(), city.name);
        }
    }
}

// ---- Gatherings outside the gates (Fleet/Gathering.h) -------------------------------------------------------------------------------
// While a real player is near the gate of Orgrimmar, Ironforge or Stormwind (all six with AllCities), a ring of world bots sits round a
// campfire and a second group stands together a little way off, the crowd outside a capital on a vanilla realm. Now and then two or
// three of a ring talk in /say: a short exchange from the scripts in Gathering.h, or with the chat model on, lines the model writes from
// the talk so far. One line at a time, a few seconds apart, then a long pause, so it reads like people talking and not like spam. The
// idle rule idle:gathering walks a bot to its seat and holds it there; this driver seats it, lights the fire, runs the talk and breaks
// the ring up after StayMin (the bots then rest from gatherings for a while, so the faces change).
namespace {

char const* GatherClassName(uint8 cls)
{
    static constexpr char const* kClass[] = { "", "warrior", "paladin", "hunter", "rogue", "priest", "death knight", "shaman", "mage",
                                               "warlock", "monk", "druid" };
    return cls < sizeof(kClass) / sizeof(kClass[0]) && kClass[cls][0] ? kClass[cls] : "adventurer";
}

char const* GatherRaceName(uint8 race)
{
    static constexpr char const* kRace[] = { "", "human", "orc", "dwarf", "night elf", "undead", "tauren", "gnome", "troll" };
    return race < sizeof(kRace) / sizeof(kRace[0]) && kRace[race][0] ? kRace[race] : "";
}

std::string GatherZoneName(uint32 zone)
{
    AreaTableEntry const* e = sAreaTableStore.LookupEntry(zone);
    return e ? std::string(e->AreaName[LOCALE_enUS]) : std::string();
}

// A said line goes through the bot's intent queue, like every other chat line of a bot.
void GatherSay(uint64 bot, std::string text)
{
    if (!Services::HasIntents(BotId(bot)))
        return;
    Intent it{};
    it.bot_id = BotId(bot);
    it.body = ChatIntent{SayChatIntent{std::move(text)}};
    Services::Intents(BotId(bot)).push(std::move(it));
}

} // namespace

void WorldBotSpawner::DriveGatherings(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    using namespace ::Playerbot::V2::Gathering;
    namespace GD = ::Playerbot::V2::GateDuels;
    constexpr uint32 kPassMs = 2000u;
    constexpr float  kCandidateYards = 200.0f;   // bots this near the gate are asked over
    constexpr float  kArrivedYards = 2.5f;
    Board& board = Board::Get();
    auto const& cfg = Services::Config().gathering();

    auto const find = [](uint64 id) -> Player*
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        return p && p->IsInWorld() ? p : nullptr;
    };
    auto const release_bot = [&](uint64 id, uint32 rest_ms)
    {
        board.Clear(id);
        if (rest_ms)
            gather_rest_[id] = now_ms + rest_ms;
        if (Player* p = find(id); p && p->GetStandState() != UNIT_STAND_STATE_STAND && p->IsAlive())
            p->SetStandState(UNIT_STAND_STATE_STAND);
    };
    auto const dissolve = [&](GatherRing& r, char const* why)
    {
        for (uint64 const id : r.members)
            release_bot(id, kRestAfterMs);
        if (!r.fire.IsEmpty())
            for (uint64 const id : r.members)
                if (Player* p = find(id); p && p->GetMap())
                {
                    if (GameObject* go = p->GetMap()->GetGameObject(r.fire))
                        go->DespawnOrUnsummon();
                    break;
                }
        if (why)
            TC_LOG_INFO("playerbot.v2", "[gathering] {} {} ended: {}", GD::kCities[r.city].name,
                        IsStreet(r.ring) ? "street chat by " + r.place : std::string(r.sit ? "campfire ring" : "standing ring"), why);
    };

    if (!cfg.enable || !Services::Config().gate_duels_enable() || anchors.empty())
    {
        for (GatherRing& r : gather_rings_) dissolve(r, nullptr);
        gather_rings_.clear();
        return;
    }
    if (last_gather_ms_ && getMSTimeDiff(last_gather_ms_, now_ms) < kPassMs)
        return;
    last_gather_ms_ = now_ms;

    // Which gates have a real player of that side close by.
    bool player_near[GD::kCityCount] = {};
    bool any = false;
    for (std::size_t ci = 0; ci < GD::kCityCount; ++ci)
    {
        if (!cfg.all_cities && !BigCity(ci)) continue;
        GateState const& gs = gate_state_[ci];
        if (gs.state != 1u) continue;
        for (Anchor const& a : anchors)
            if (a.map == GD::kCities[ci].map && a.alliance == GD::kCities[ci].alliance && Dist2D(a.x, a.y, gs.x, gs.y) <= cfg.radius)
            { player_near[ci] = true; any = true; break; }
    }

    // The rings in progress.
    for (auto it = gather_rings_.begin(); it != gather_rings_.end();)
    {
        GatherRing& r = *it;
        char const* end_why = nullptr;
        if (IsStreet(r.ring))
        {
            bool player_close = false;
            for (Anchor const& a : anchors)
                player_close = player_close || (a.map == GD::kCities[r.city].map && a.alliance == GD::kCities[r.city].alliance && Dist2D(a.x, a.y, r.cx, r.cy) <= 90.0f);
            if (!player_close) end_why = "no player near any more";
        }
        else if (!player_near[r.city]) end_why = "no player near the gate";
        if (!end_why && now_ms >= r.until_ms) end_why = "time to move on";
        if (!end_why)
        {
            // Members that are gone, fighting, grouped, duelling or dead leave the ring.
            for (auto m = r.members.begin(); m != r.members.end();)
            {
                Player* p = find(*m);
                bool const keep = p && p->IsAlive() && !p->IsInCombat() && !p->GetGroup() && !p->duel && p->GetMapId() == GD::kCities[r.city].map
                    && !p->IsBeingTeleported() && board.Has(*m);
                if (keep) { ++m; continue; }
                release_bot(*m, p ? kRestAfterMs : 0u);
                r.talkers.erase(std::remove(r.talkers.begin(), r.talkers.end(), *m), r.talkers.end());
                m = r.members.erase(m);
            }
            if (r.members.size() < 2u) end_why = "too few left";
        }
        if (end_why)
        {
            dissolve(r, end_why);
            it = gather_rings_.erase(it);
            continue;
        }

        // Seat the ones that arrived: face the middle, sit down at the fire.
        std::vector<Player*> arrived;
        for (uint64 const id : r.members)
        {
            Player* p = find(id);
            Spot s;
            if (!p || !board.Find(id, s)) continue;
            if (p->isMoving() && p->GetStandState() != UNIT_STAND_STATE_STAND)
                p->SetStandState(UNIT_STAND_STATE_STAND);   // called away by another rule: get up first
            if (p->isMoving() || Dist2D(p->GetPositionX(), p->GetPositionY(), s.x, s.y) > kArrivedYards) continue;
            float const off = std::fabs(Position::NormalizeOrientation(p->GetOrientation() - s.face));
            if (off > 0.35f && off < 6.0f)
                p->SetFacingTo(s.face);
            if (s.sit && p->GetStandState() != UNIT_STAND_STATE_SIT)
                p->SetStandState(UNIT_STAND_STATE_SIT);
            arrived.push_back(p);
        }

        // The fire: the first time, a bot lights a Basic Campfire (818) and the object it makes is learnt; from then on the driver puts
        // that object in the middle of the ring for as long as the ring lasts.
        if (r.sit && cfg.campfire && campfire_entry_ != UINT32_MAX && arrived.size() >= 2u)
        {
            Player* keeper = arrived.front();
            GameObject* fire = r.fire.IsEmpty() ? nullptr : keeper->GetMap()->GetGameObject(r.fire);
            if (!fire && campfire_entry_)
            {
                float const ang = keeper->GetOrientation();
                uint32 const secs = (r.until_ms > now_ms ? (r.until_ms - now_ms) / 1000u : 0u) + 30u;
                if (GameObject* go = keeper->SummonGameObject(campfire_entry_, r.cx, r.cy, r.cz, ang, QuaternionData::fromEulerAnglesZYX(ang, 0.f, 0.f), Seconds(secs)))
                    r.fire = go->GetGUID();
            }
            else if (!fire && !r.fire_try_ms)
            {
                r.fire_try_ms = now_ms;
                if (sSpellMgr->GetSpellInfo(818u, DIFFICULTY_NONE))
                    keeper->CastSpell(keeper, 818u, true);
                else
                {
                    campfire_entry_ = UINT32_MAX;
                    TC_LOG_INFO("playerbot.v2", "[gathering] no Basic Campfire spell (818) in this data: gatherings have no fire");
                }
            }
            else if (!fire && !campfire_entry_ && getMSTimeDiff(r.fire_try_ms, now_ms) >= 2000u)
            {
                GameObject* go = keeper->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_SPELL_FOCUS, 8.0f);
                if (go && go->GetSpawnId())
                    go = nullptr;   // a placed object of the world (a forge, a cooking fire of a camp), not the one just lit
                if (go)
                {
                    campfire_entry_ = go->GetEntry();
                    go->DespawnOrUnsummon();   // the next pass puts one in the middle of the ring
                    TC_LOG_INFO("playerbot.v2", "[gathering] campfire object is entry {}", campfire_entry_);
                }
                else
                {
                    campfire_entry_ = UINT32_MAX;
                    TC_LOG_INFO("playerbot.v2", "[gathering] {} lit a campfire (818) but no fire object appeared: gatherings have no fire", keeper->GetName());
                }
            }
        }

        // The talk.
        if (now_ms >= r.next_line_ms)
        {
            GD::City const& city = GD::kCities[r.city];
            auto const end_talk = [&]()
            {
                r.script = -1; r.model = false; r.waiting = false; r.chat.clear();
                r.next_line_ms = now_ms + cfg.talk_gap_ms + urand(0, cfg.talk_gap_ms / 2u);
            };
            auto const emote = [&](Player* p, uint16 e)
            {
                if (e && !r.sit && p) p->HandleEmoteCommand(Emote(e));
            };
            auto const start_script = [&]()
            {
                std::vector<std::size_t> pool = ScriptsFor(city.alliance, r.talkers.size(), IsStreet(r.ring));
                std::vector<std::size_t> fresh;
                for (std::size_t i : pool)
                    if (std::find(r.recent.begin(), r.recent.end(), uint32(i)) == r.recent.end()) fresh.push_back(i);
                if (fresh.empty()) { r.recent.clear(); fresh = pool; }
                if (fresh.empty()) { end_talk(); return; }
                r.script = int32(fresh[urand(0, uint32(fresh.size() - 1))]);
                r.line = 0;
                r.recent.push_back(uint32(r.script));
                if (r.recent.size() > 8u) r.recent.erase(r.recent.begin());
            };

            if (r.script < 0 && !r.model)
            {
                if (arrived.size() < 2u)
                    r.next_line_ms = now_ms + 5000u;
                else
                {
                    // Two or three of the seated ones talk this time.
                    r.talkers.clear();
                    for (Player* p : arrived) r.talkers.push_back(p->GetGUID().GetCounter());
                    for (std::size_t i = r.talkers.size(); i > 1; --i) std::swap(r.talkers[i - 1], r.talkers[urand(0, uint32(i - 1))]);
                    r.talkers.resize(std::min<std::size_t>(r.talkers.size(), urand(2, 3)));
                    if (cfg.use_chat_model && BotChatModel::SayOn())
                    {
                        r.model = true; r.waiting = false; r.chat.clear();
                        r.turns_left = 3u + urand(0, 3);
                        r.topic = urand(0, uint32((IsStreet(r.ring) ? kStreetTopicCount : kTopicCount) - 1));
                        r.key = (uint64(0x6A7Bu) << 48) | (++gather_seq_ & 0xFFFFFFFFFFFFull);
                    }
                    else
                        start_script();
                }
            }

            if (r.model)
            {
                for (auto const& [name, line] : BotChatModel::TakeSaid(r.key))
                {
                    r.chat += name + ": " + line + "\n";
                    r.waiting = false;
                    if (r.turns_left) --r.turns_left;
                    r.next_line_ms = now_ms + LinePause(line.size(), urand(0, 0xFFFF));
                    // Face to face: the speaker's gesture fits the line (a question, a laugh, a yes, a no), and now and then one of the
                    // others laughs along at a funny one.
                    uint16 const gesture = GestureFor(line);
                    bool laughed = false;
                    for (uint64 const id : r.talkers)
                        if (Player* p = find(id))
                        {
                            if (name == p->GetName()) emote(p, gesture);
                            else if (gesture == kGestureLaugh && !laughed && urand(0, 1)) { emote(p, kGestureLaugh); laughed = true; }
                        }
                }
                if (r.waiting)
                {
                    if (getMSTimeDiff(r.asked_ms, now_ms) > 25000u) end_talk();   // the model is slow or busy: the talk stops there
                }
                else if (!r.turns_left)
                    end_talk();
                else if (now_ms >= r.next_line_ms)
                {
                    // The talkers take turns; a bot never answers its own line.
                    std::size_t const turn = std::count(r.chat.begin(), r.chat.end(), '\n');
                    Player* sp = r.talkers.empty() ? nullptr : find(r.talkers[turn % r.talkers.size()]);
                    if (!sp)
                        end_talk();
                    else
                    {
                        std::string others;
                        for (uint64 const id : r.talkers)
                            if (Player* o = find(id); o && o != sp)
                                others += (others.empty() ? "" : " and ") + std::string(o->GetName());
                        std::string sys = "You are " + sp->GetName() + ", a level " + std::to_string(sp->GetLevel()) + " ";
                        if (char const* race = GatherRaceName(sp->GetRace()); *race) sys += std::string(race) + " ";
                        sys += GatherClassName(sp->GetClass());
                        sys += " playing World of Warcraft Classic (the original 2004 game).";
                        if (IsStreet(r.ring))
                        {
                            sys += " You are standing near " + r.place + " in " + city.name + ", face to face with " + (others.empty() ? std::string("a friend") : others)
                                + ", chatting in /say while people walk past.";
                            sys += std::string(" The talk is about ") + kStreetTopics[r.topic % kStreetTopicCount] + ".";
                        }
                        else
                        {
                            sys += std::string(" You are ") + (r.sit ? "sitting around a campfire" : "standing with a few others") + " just outside the gates of "
                                + city.name + ", in " + GatherZoneName(sp->GetZoneId()) + ", chatting in /say with " + (others.empty() ? std::string("a friend") : others) + ".";
                            sys += std::string(" The talk is about ") + kTopics[r.topic % kTopicCount] + ".";
                        }
                        sys += " Write only your own next line: one short line under 15 words, casual, like a player typing in game chat, no emojis,"
                               " no actions in asterisks, no quotation marks, no name prefix. You may answer the others or tease them by name."
                               " Never say you are a bot or a program. Don't invent quests, items or places.";
                        sys += " " + ::Playerbot::V2::World::VanillaLevelPlaces(uint8(sp->GetLevel()));
                        if (!Services::Config().chat_model().extra_prompt.empty())
                            sys += " " + Services::Config().chat_model().extra_prompt;
                        if (BotChatModel::AskSay(sp, sys, r.chat, r.key, cfg.model_per_minute))
                        {
                            r.waiting = true;
                            r.asked_ms = now_ms;
                        }
                        else if (r.chat.empty())
                        {
                            r.model = false;   // the model is busy or over its cap: this talk is a script instead
                            start_script();
                        }
                        else
                            end_talk();
                    }
                }
            }

            if (!r.model && r.script >= 0 && now_ms >= r.next_line_ms)
            {
                Script const& sc = Scripts()[std::size_t(r.script)];
                if (r.line >= sc.lines.size())
                    end_talk();
                else
                {
                    Line const& ln = sc.lines[r.line++];
                    Player* sp = ln.who < r.talkers.size() ? find(r.talkers[ln.who]) : nullptr;
                    if (!sp)
                        end_talk();   // a talker left: the rest of the exchange would not make sense
                    else
                    {
                        std::vector<std::string> names;
                        for (uint64 const id : r.talkers)
                            if (Player* o = find(id)) names.push_back(o->GetName()); else names.push_back("friend");
                        std::string text = Fill(ln.text, city.name, GatherZoneName(sp->GetZoneId()), names);
                        emote(sp, ln.emote);
                        r.next_line_ms = now_ms + LinePause(text.size(), urand(0, 0xFFFF));
                        GatherSay(sp->GetGUID().GetCounter(), std::move(text));
                        if (r.line >= sc.lines.size())
                            end_talk();
                    }
                }
            }
        }
        ++it;
    }
    FormStreetChats(now_ms, anchors);
    if (!any)
        return;

    // New rings where a player is near and the gate has fewer than it should.
    std::size_t want_fire = 0, want_stand = 0;
    SplitRings(cfg.bots_per_gate, want_fire, want_stand);
    bool need[GD::kCityCount] = {};
    bool any_need = false;
    for (std::size_t ci = 0; ci < GD::kCityCount; ++ci)
    {
        if (!player_near[ci] || (gather_form_ms_[ci] && getMSTimeDiff(gather_form_ms_[ci], now_ms) < 20000u)) continue;
        uint32 have = 0;
        for (GatherRing const& r : gather_rings_) if (r.city == ci && !IsStreet(r.ring)) ++have;
        if (have < uint32(want_fire ? 1 : 0) + uint32(want_stand ? 1 : 0)) { need[ci] = true; any_need = true; }
    }
    if (!any_need)
        return;

    if (gather_rest_.size() > 512u)
        for (auto it = gather_rest_.begin(); it != gather_rest_.end();)
            it = (now_ms > it->second) ? gather_rest_.erase(it) : std::next(it);

    struct Cand { uint64 id; Player* p; float d; };
    std::vector<Cand> cands[GD::kCityCount];
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = find(uint64(id));
        if (!GatherFree(id, p, now_ms)) return;
        for (std::size_t ci = 0; ci < GD::kCityCount; ++ci)
        {
            if (!need[ci] || p->GetMapId() != GD::kCities[ci].map || (p->GetTeam() == ALLIANCE) != GD::kCities[ci].alliance) continue;
            float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), gate_state_[ci].x, gate_state_[ci].y);
            if (d <= kCandidateYards) { cands[ci].push_back({ uint64(id), p, d }); return; }
        }
    });

    for (std::size_t ci = 0; ci < GD::kCityCount; ++ci)
    {
        if (!need[ci]) continue;
        GD::City const& city = GD::kCities[ci];
        GateState const& gs = gate_state_[ci];
        gather_form_ms_[ci] = now_ms;
        std::vector<Cand>& cs = cands[ci];
        std::sort(cs.begin(), cs.end(), [](Cand const& a, Cand const& b) { return a.d < b.d; });
        for (uint32 ring = 0; ring < 2u; ++ring)
        {
            std::size_t const want = ring == 0 ? want_fire : want_stand;
            if (!want) continue;
            bool taken_ring = false;
            for (GatherRing const& r : gather_rings_) if (r.city == ci && r.ring == ring) taken_ring = true;
            if (taken_ring) continue;
            if (cs.size() < kMinRing)
            {
                if (getMSTimeDiff(gather_why_ms_[ci], now_ms) >= 60000u)
                {
                    gather_why_ms_[ci] = now_ms;
                    TC_LOG_INFO("playerbot.v2", "[gathering] {} gate at ({:.0f},{:.0f}): {} free bot(s) within {:.0f} yd, {} needed", city.name, gs.x, gs.y,
                        uint32(cs.size()), kCandidateYards, uint32(kMinRing));
                }
                break;
            }
            Player* first = cs.front().p;
            Pt const c = RingCenter(gs.x, gs.y, ring);
            float const cz = first->GetMap()->GetHeight(first->GetPhaseShift(), c.x, c.y, gs.z + 8.0f, true, 25.0f);
            uint32 used = 0;
            if (cz <= INVALID_HEIGHT || std::fabs(cz - gs.z) > 10.0f || first->GetMap()->IsInWater(first->GetPhaseShift(), c.x, c.y, cz)
                || !ReachableChain(first, c.x, c.y, cz, 3u, used))
            {
                if (getMSTimeDiff(gather_why_ms_[ci], now_ms) >= 60000u)
                {
                    gather_why_ms_[ci] = now_ms;
                    TC_LOG_INFO("playerbot.v2", "[gathering] {} ring {} spot ({:.0f},{:.0f}) is not open ground a bot can walk to", city.name, ring, c.x, c.y);
                }
                continue;
            }
            GatherRing r;
            r.city = uint32(ci); r.ring = ring; r.sit = ring == 0; r.cx = c.x; r.cy = c.y; r.cz = cz;
            r.since_ms = now_ms;
            r.until_ms = now_ms + cfg.stay_ms - cfg.stay_ms / 4u + urand(0, cfg.stay_ms / 2u);
            r.next_line_ms = now_ms + 20000u + urand(0, 15000u);   // time to walk over and sit before anyone talks
            std::size_t const n = std::min(want, cs.size());
            std::size_t seated = 0;
            for (std::size_t i = 0; i < cs.size() && seated < n;)
            {
                float face = 0.f;
                Pt const st = Seat(c, seated, n, r.sit ? kSitRadius : kStandRadius, face);
                float const sz = cs[i].p->GetMap()->GetHeight(cs[i].p->GetPhaseShift(), st.x, st.y, cz + 4.0f, true, 10.0f);
                if (sz <= INVALID_HEIGHT || std::fabs(sz - cz) > 3.0f) { ++seated; continue; }   // that seat is a rock or a hole: leave it empty
                board.Set(cs[i].id, Spot{ uint32(ci), ring, st.x, st.y, sz, face, r.sit });
                r.members.push_back(cs[i].id);
                cs.erase(cs.begin() + std::ptrdiff_t(i));
                ++seated;
            }
            if (r.members.size() < 2u)
            {
                for (uint64 const id : r.members) board.Clear(id);
                continue;
            }
            std::string names;
            for (uint64 const id : r.members) if (Player* p = find(id)) names += std::string(names.empty() ? "" : ", ") + p->GetName();
            TC_LOG_INFO("playerbot.v2", "[gathering] {} {} ring at ({:.0f},{:.0f}): {}", city.name, r.sit ? "campfire" : "standing", c.x, c.y, names);
            gather_rings_.push_back(std::move(r));
        }
    }
}

bool WorldBotSpawner::GatherFree(BotId id, Player* p, uint32 now_ms) const
{
    namespace GD = ::Playerbot::V2::GateDuels;
    if (!p || !p->IsAlive() || !p->GetMap() || p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport() || p->IsMounted()) return false;
    if (!owner_.IsAmbientBot(id, p) || p->GetGroup() || p->InBattleground() || p->InBattlegroundQueue() || p->GetMap()->IsDungeon()) return false;
    if (!owner_.SetupDone(uint64(id)) || pending_.contains(uint64(id)) || seeding_.contains(uint64(id))) return false;
    if (p->IsInCombat() || p->duel || p->GetLevel() < 5 || p->IsInWater()) return false;
    if (::Playerbot::V2::Gathering::Board::Get().Has(uint64(id))) return false;
    GD::Assignment taken;
    if (GD::Board::Get().Find(uint64(id), taken)) return false;
    if (auto const r = gather_rest_.find(uint64(id)); r != gather_rest_.end() && now_ms < r->second) return false;
    return true;
}

// Street chats (Gathering.h, Playerbot.World.Gathering.CityChats): in a capital, by a bank, auctioneer, innkeeper, shop or mailbox
// within 60 yards of a real player of that side, two to four free world bots of the city stand together facing each other; the talk
// itself is DriveGatherings'. At most one new chat per city every 30 s, CityChats at once, kStreetApart between two.
void WorldBotSpawner::FormStreetChats(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    using namespace ::Playerbot::V2::Gathering;
    namespace GD = ::Playerbot::V2::GateDuels;
    namespace TV = ::Playerbot::V2::Travel;
    auto const& cfg = Services::Config().gathering();
    if (!cfg.city_chats || !TV::TownNpcIndex::Instance().IsInitialized())
        return;
    constexpr float kNearPlayer = 60.0f, kCandidateYards = 150.0f;
    constexpr uint32 kPlaceFlags = UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_VENDOR |
                                   UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER;
    Board& board = Board::Get();
    auto const find = [](uint64 id) -> Player*
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        return p && p->IsInWorld() ? p : nullptr;
    };
    for (std::size_t ci = 0; ci < GD::kCityCount; ++ci)
    {
        GD::City const& city = GD::kCities[ci];
        if (street_form_ms_[ci] && getMSTimeDiff(street_form_ms_[ci], now_ms) < 30000u)
            continue;
        uint32 have = 0;
        for (GatherRing const& r : gather_rings_) if (r.city == ci && IsStreet(r.ring)) ++have;
        if (have >= cfg.city_chats)
            continue;
        Anchor const* who = nullptr;
        for (Anchor const& a : anchors)
            if (a.map == city.map && a.zone == city.zone && a.alliance == city.alliance) { who = &a; break; }
        if (!who)
            continue;
        street_form_ms_[ci] = now_ms;

        // The places near him: service NPCs of his side and mailboxes, none too close to a chat already there.
        struct Place { float x, y, z; std::string name; };
        std::vector<Place> places;
        auto const free_place = [&](float x, float y)
        {
            for (GatherRing const& r : gather_rings_)
                if (r.city == ci && Dist2D(r.cx, r.cy, x, y) < kStreetApart + kStreetFromNpc) return false;
            return true;
        };
        auto const& index = TV::TownNpcIndex::Instance();
        index.ForEachNear(city.map, who->x, who->y, kNearPlayer, [&](TV::TownNpc const& n)
        {
            if (!(n.npc_flags & kPlaceFlags) || !n.usable_by(city.alliance) || std::fabs(n.z - who->z) > 15.0f || !free_place(n.x, n.y)) return;
            char const* name = (n.npc_flags & UNIT_NPC_FLAG_BANKER) ? "the bank" : (n.npc_flags & UNIT_NPC_FLAG_AUCTIONEER) ? "the auction house"
                             : (n.npc_flags & UNIT_NPC_FLAG_INNKEEPER) ? "the inn" : (n.npc_flags & UNIT_NPC_FLAG_FLIGHTMASTER) ? "the flight master"
                             : (n.npc_flags & UNIT_NPC_FLAG_TRAINER) ? "a trainer" : "a shop";
            places.push_back({ n.x, n.y, n.z, name });
        });
        index.ForEachMailboxNear(city.map, who->x, who->y, kNearPlayer, [&](TV::TownNpc const& n)
        {
            if (std::fabs(n.z - who->z) <= 15.0f && free_place(n.x, n.y))
                places.push_back({ n.x, n.y, n.z, "the mailbox" });
        });
        if (places.empty())
            continue;
        Place const& pl = places[urand(0, uint32(places.size() - 1))];
        Pt const c = StreetCenter(pl.x, pl.y, who->x, who->y);

        // Free bots of the city.
        struct Cand { uint64 id; Player* p; float d; };
        std::vector<Cand> cs;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            Player* p = find(uint64(id));
            if (!GatherFree(id, p, now_ms) || p->GetMapId() != city.map || p->GetZoneId() != city.zone || (p->GetTeam() == ALLIANCE) != city.alliance) return;
            float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), c.x, c.y);
            if (d <= kCandidateYards) cs.push_back({ uint64(id), p, d });
        });
        if (cs.size() < kStreetMin)
            continue;
        std::sort(cs.begin(), cs.end(), [](Cand const& a, Cand const& b) { return a.d < b.d; });
        Player* first = cs.front().p;
        float const cz = first->GetMap()->GetHeight(first->GetPhaseShift(), c.x, c.y, pl.z + 5.0f, true, 25.0f);
        uint32 used = 0;
        if (cz <= INVALID_HEIGHT || std::fabs(cz - pl.z) > 4.0f || first->GetMap()->IsInWater(first->GetPhaseShift(), c.x, c.y, cz)
            || !ReachableChain(first, c.x, c.y, cz, 3u, used))
            continue;   // a counter, a ledge or water there: another place next time

        GatherRing r;
        r.city = uint32(ci);
        r.ring = kStreetRing + (++street_seq_ % 100000u);
        r.sit = false;
        r.place = pl.name;
        r.cx = c.x; r.cy = c.y; r.cz = cz;
        r.since_ms = now_ms;
        r.until_ms = now_ms + urand(kStreetStayMinMs, kStreetStayMaxMs);
        r.next_line_ms = now_ms + 12000u + urand(0, 8000u);   // time to walk over first
        std::size_t const n = std::min<std::size_t>(cs.size(), urand(uint32(kStreetMin), uint32(kStreetMax)));
        for (std::size_t i = 0, seat = 0; i < cs.size() && r.members.size() < n && seat < n; ++seat)
        {
            float face = 0.f;
            Pt const st = Seat(c, seat, n, kStreetRadius, face);
            float const sz = cs[i].p->GetMap()->GetHeight(cs[i].p->GetPhaseShift(), st.x, st.y, cz + 3.0f, true, 8.0f);
            if (sz <= INVALID_HEIGHT || std::fabs(sz - cz) > 1.5f) continue;   // a step or a wall there: leave that place empty
            board.Set(cs[i].id, Spot{ uint32(ci), r.ring, st.x, st.y, sz, face, false });
            r.members.push_back(cs[i].id);
            ++i;
        }
        if (r.members.size() < kStreetMin)
        {
            for (uint64 const id : r.members) board.Clear(id);
            continue;
        }
        std::string names;
        for (uint64 const id : r.members) if (Player* p = find(id)) names += std::string(names.empty() ? "" : ", ") + p->GetName();
        TC_LOG_INFO("playerbot.v2", "[gathering] {} street chat by {} at ({:.0f},{:.0f}): {}", city.name, r.place, c.x, c.y, names);
        gather_rings_.push_back(std::move(r));
    }
}

// Residents that stand where they cannot live: in lava (at once), or in a town on ground with no way out of it
// (no complete path to the nearest player three checks running). They are placed again the way a new resident is, next to the nearest player, which now refuses such
// spots. This clears what older builds left in the Great Forge pit (and what sleeps there and wakes); the placement checks stop
// new ones. A few bots per pass, each bot looked at every kStrandedCheckMs.
void WorldBotSpawner::DriveStranded(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    constexpr uint32 kPassMs = 2000u;
    constexpr uint32 kProbesPerPass = 12u;     // path probes (the costly ones) per pass; lava checks are cheap. Three a pass checked a bot once in two minutes or worse once 40 bots needed the full chain
    constexpr float  kTownPathRange = 200.0f;  // a town's player this close gives the reference for "a way out"
    if (last_stranded_ms_ && getMSTimeDiff(last_stranded_ms_, now_ms) < kPassMs)
        return;
    last_stranded_ms_ = now_ms;
    if (anchors.empty())
        return;

    uint32 probes = 0;
    std::vector<uint64> seen;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& entry)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !p->GetMap() || p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport())
            return;
        if (!owner_.IsAmbientBot(id, p) || p->GetGroup() || p->IsInCombat() || p->InBattleground() || p->InBattlegroundQueue()
            || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())
            return;
        if (!owner_.SetupDone(uint64(id)) || pending_.contains(uint64(id)) || seeding_.contains(uint64(id)))
            return;
        StrandedState& st = stranded_[uint64(id)];
        float const prev_x = st.x, prev_y = st.y, prev_z = st.z;   // where the last check found it (zero before the first)
        if (!StrandedDue(st, now_ms))
            return;

        // The nearest player on the bot's map: the reference for a way out, and where a stranded bot is put again.
        Anchor const* near_a = nullptr;
        float near_d = 1e9f;
        for (Anchor const& a : anchors)
        {
            if (a.map != p->GetMapId()) continue;
            float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), a.x, a.y);
            if (d < near_d) { near_d = d; near_a = &a; }
        }
        if (!near_a)
            return;   // nobody on this map: the despawn pass logs it out
        // The nearest player is somewhere else entirely (the only player went from Stormwind to Deathknell): this bot is no resident of
        // theirs any more and the despawn pass logs it out. Placing it next to them took 33 stranded Stormwind bots of any level and faction
        // to Deathknell (PC 26 Oct).
        if (near_d > cfg_.despawn_radius)
            return;

        float const px = p->GetPositionX(), py = p->GetPositionY(), pz = p->GetPositionZ();
        bool const magma = InMagma(p->GetMap(), p->GetPhaseShift(), px, py, pz + 0.05f);
        bool path_out = true;
        bool const in_town = ::Playerbot::V2::World::IsVanillaCapitalZone(p->GetZoneId());
        if (!magma && in_town && near_a->zone == p->GetZoneId() && near_d <= kTownPathRange)
        {
            if (probes >= kProbesPerPass)
                return;   // next pass: st stays due
            uint32 used = 0;
            path_out = ReachableChain(p, near_a->x, near_a->y, near_a->z, 4u, used);
            probes += std::max<uint32>(1u, used);
        }
        // No walk to the player, or nobody close by: the town's own meeting places are the reference. A city on two levels joined only by
        // lifts (the Undercity under the Ruins of Lordaeron) has no navmesh path between them, so every resident below a player in the
        // ruins was "stranded" and put next to him (0.1.25: Emmanu at the Undercity's vendors, z -43).
        if (!magma && in_town && (!path_out || !(near_a->zone == p->GetZoneId() && near_d <= kTownPathRange)))
        {
            // A bot on a lower layer under the city (a bot at z 75 and 59 below the Trade District's z 97, same x/y) reaches none of them.
            // It must reach at least one of the three nearest.
            std::vector<::Playerbot::V2::Fleet::GatherSpot> spots =
                ::Playerbot::V2::Fleet::TownGatherSpots(p->GetMapId(), px, py, 150.0f, p->GetTeam() == ALLIANCE);
            if (spots.empty())   // a far corner of the city (Dutchess, the wall at the north-west): no meeting place within 150 y, so look wider
                spots = ::Playerbot::V2::Fleet::TownGatherSpots(p->GetMapId(), px, py, 600.0f, p->GetTeam() == ALLIANCE);
            std::sort(spots.begin(), spots.end(), [&](auto const& a, auto const& b)
                { return Dist2D(a.x, a.y, px, py) < Dist2D(b.x, b.y, px, py); });
            if (spots.size() > 3u)
                spots.resize(3u);
            if (!spots.empty())
            {
                if (probes >= kProbesPerPass)
                    return;
                path_out = false;
                for (auto const& g : spots)
                {
                    uint32 used = 0;
                    bool const ok = ReachableChain(p, g.x, g.y, g.z, 4u, used);
                    probes += std::max<uint32>(1u, used);
                    if (ok) { path_out = true; break; }
                }
            }
        }
        // Every path call from this spot has been failing for minutes (a pocket with void all round it: Hellice, 3720 NoPath in an hour,
        // pacing a few yards so the idle watch never saw her stand still): as cut off as a bot whose probe finds no way out.
        bool const storm = !magma && path_out && ::Playerbot::BotMovement::NoPathStorm(p);
        if (storm)
            path_out = false;
        if (!StrandedUpdate(st, magma, path_out, px, py, pz, now_ms))
            return;

        Player* ap = ObjectAccessor::FindConnectedPlayer(near_a->guid);
        if (!ap || !ap->IsInWorld() || !PlaceNear(p, ap))
            return;
        placed_[uint64(id)] = Placed{ now_ms };
        ::Playerbot::BotMovement::ClearNoPath(p);
        ++n_stranded_moved_;
        // How it got there, so the cause can be read from the log: the rule that last moved it, and the spot the previous check found it
        // on (a bot that walked in shows a previous spot nearby; one that was put there or woke there shows none or a far one).
        char const* last_rule = entry.ai && entry.ai->last_rule_fired() ? entry.ai->last_rule_fired() : "-";
        TC_LOG_INFO("playerbot.v2", "[world] {} ({}) was {} at ({:.0f},{:.0f},{:.0f}) in zone {}, placed again next to {} | last_rule={} previous_check=({:.0f},{:.0f},{:.0f})",
                    p->GetName(), uint64(id), magma ? "standing in lava" : storm ? "on ground where every path call fails" : "on ground with no way out", px, py, pz, p->GetZoneId(), ap->GetName(),
                    last_rule, prev_x, prev_y, prev_z);
    });
    // Forget bots that are gone.
    if (stranded_.size() > 2048u)
        for (auto it = stranded_.begin(); it != stranded_.end();)
            it = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(it->first)) ? std::next(it) : stranded_.erase(it);
}

// The stuck ledger. The idle watch (Bot/IdleWatch.h) finds a bot standing for 45 s again and again and sends it to a new place each
// time; a bot that still never moves (Dexie: 44 strikes) cannot be helped by a new place. Here, on the world thread, the facts the bot
// worker cannot see are written down once a minute per bot: where it stands against the navmesh, what holds it (root, stun, a
// movement generator), and whether the place it was sent to can be reached from where it stands. A bot standing off the navmesh is
// put onto the nearest walkable ground (the same fix a sleeper gets at wake): every path from a spot with no polygon fails in place.
// The out-of-sight swap: a world bot the idle watch has found standing for no reason IdleSwapStrikes times running, where no player can
// see it, is placed again by PlaceNear around the nearest player: out of his sight and walking in, like a new arrival. A bot standing
// in view is left to the idle watch's own walks, so nobody sees a bot vanish.
void WorldBotSpawner::DriveIdleSwap(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    constexpr uint32 kPassMs = 5000u, kSwapsPerPass = 2u;
    constexpr float kSeenYards = 100.0f;
    if (!cfg_.idle_swap || anchors.empty())
        return;
    if (last_swap_pass_ms_ && getMSTimeDiff(last_swap_pass_ms_, now_ms) < kPassMs)
        return;
    last_swap_pass_ms_ = now_ms;
    std::vector<std::pair<uint64, uint32>> picks;   // bot, strikes
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& entry)
    {
        if (picks.size() >= kSwapsPerPass * 4u || !entry.ai)
            return;
        ::Playerbot::V2::IdleWatch const w = entry.ai->idle_watch();   // a copy: the bot worker writes it
        if (w.strikes < cfg_.idle_swap_strikes || entry.ai->walk_order().active.load(std::memory_order_acquire))
            return;
        if (auto it = swap_ms_.find(uint64(id)); it != swap_ms_.end() && getMSTimeDiff(it->second, now_ms) < cfg_.idle_swap_cd_ms)
            return;
        picks.push_back({ uint64(id), w.strikes });
    });
    uint32 done = 0;
    for (auto const& [id, strikes] : picks)
    {
        if (done >= kSwapsPerPass)
            break;
        if (leaving_.contains(id) || pending_.contains(id))
            continue;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !p->GetMap() || p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport())
            continue;
        if (!owner_.IsAmbientBot(BotId(id), p) || p->GetGroup() || p->IsInCombat() || p->InBattleground() || p->InBattlegroundQueue()
            || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena() || p->IsSitState())
            continue;
        Anchor const* nearest = nullptr;
        float best = cfg_.activity_radius;
        bool seen = false;
        for (Anchor const& a : anchors)
        {
            if (a.map != p->GetMapId()) continue;
            float const d = Dist2D(p->GetPositionX(), p->GetPositionY(), a.x, a.y);
            if (d < best) { best = d; nearest = &a; }
            if (d > kSeenYards || seen) continue;
            Player* ap = ObjectAccessor::FindConnectedPlayer(a.guid);
            if (ap && ap->IsInWorld() && ap->IsWithinLOS(p->GetPositionX(), p->GetPositionY(), p->GetPositionZ() + 1.5f))
                seen = true;
        }
        if (!nearest || seen)
            continue;
        Player* anchor = ObjectAccessor::FindConnectedPlayer(nearest->guid);
        if (!anchor || !anchor->IsInWorld() || anchor->GetMapId() != p->GetMapId())
            continue;
        swap_ms_[id] = now_ms ? now_ms : 1u;
        float const fx = p->GetPositionX(), fy = p->GetPositionY();
        if (!PlaceNear(p, anchor))
            continue;
        ++done;
        ++n_idle_swaps_;
        TC_LOG_INFO("playerbot.v2", "[idle_swap] {} stood unseen (idle strikes {}) at ({:.0f},{:.0f}) zone {}: placed again out of {}'s sight, walks in",
                    p->GetName(), strikes, fx, fy, p->GetZoneId(), anchor->GetName());
    }
    if (swap_ms_.size() > 2048u)
        for (auto it = swap_ms_.begin(); it != swap_ms_.end();)
            it = getMSTimeDiff(it->second, now_ms) > cfg_.idle_swap_cd_ms ? swap_ms_.erase(it) : std::next(it);
}

void WorldBotSpawner::DriveStuckLedger(uint32 now_ms)
{
    constexpr uint32 kPassMs = 5000u, kPerBotMs = 60000u, kMinStrikes = 3u, kBotsPerPass = 3u;
    if (last_ledger_ms_ && getMSTimeDiff(last_ledger_ms_, now_ms) < kPassMs)
        return;
    last_ledger_ms_ = now_ms;
    uint32 done = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& entry)
    {
        if (done >= kBotsPerPass || !entry.ai)
            return;
        ::Playerbot::V2::IdleWatch const w = entry.ai->idle_watch();   // a copy: the bot worker writes it
        if (w.strikes < kMinStrikes)
            return;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !p->GetMap() || p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport())
            return;
        if (!owner_.IsAmbientBot(id, p) || p->GetGroup() || p->IsInCombat() || p->InBattleground() || p->InBattlegroundQueue()
            || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())
            return;
        auto it = ledger_ms_.find(uint64(id));
        if (it != ledger_ms_.end() && getMSTimeDiff(it->second, now_ms) < kPerBotMs)
            return;
        ledger_ms_[uint64(id)] = now_ms;
        ++done;

        float const px = p->GetPositionX(), py = p->GetPositionY(), pz = p->GetPositionZ();
        Position near_mesh, wide_mesh;
        bool const on_mesh = ::Playerbot::BotMovement::NearestNavPoint(p, px, py, pz, 3.0f, 4.0f, near_mesh);
        bool wide = false;
        if (!on_mesh)
            wide = ::Playerbot::BotMovement::NearestNavPoint(p, px, py, pz, 40.0f, 12.0f, wide_mesh);
        char const* reach = "no_target";
        if (w.has_target)
        {
            uint32 used = 0;
            reach = ReachableChain(p, w.tx, w.ty, w.tz, 4u, used) ? "reachable" : "unreachable";
        }
        float const ground = p->GetMap()->GetHeight(p->GetPhaseShift(), px, py, pz + 3.0f, true, 20.0f);
        TC_LOG_INFO("playerbot.v2",
            "[stuck_ledger] {} ({}) zone {} at ({:.1f},{:.1f},{:.1f}) strikes={} on_mesh={} {} ground_z={:.1f} rooted={} stunned={} held_state={} "
            "falling={} swimming={} move_gen={} run_speed={:.1f} nopath_storm={} target=({:.0f},{:.0f},{:.0f}) {} last_rule={}",
            p->GetName(), uint64(id), p->GetZoneId(), px, py, pz, w.strikes, on_mesh ? "yes" : "NO",
            on_mesh ? "" : (wide ? Trinity::StringFormat("(walkable ground {:.0f} y away)", std::hypot(wide_mesh.GetPositionX() - px, wide_mesh.GetPositionY() - py)) : std::string("(none within 40 y)")),
            ground > INVALID_HEIGHT ? ground : -999.f, p->HasUnitMovementFlag(MOVEMENTFLAG_ROOT) ? 1 : 0, p->HasUnitState(UNIT_STATE_STUNNED) ? 1 : 0,
            p->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING | UNIT_STATE_CONTROLLED) ? 1 : 0,
            p->IsFalling() ? 1 : 0, p->IsInWater() ? 1 : 0, uint32(p->GetMotionMaster()->GetCurrentMovementGeneratorType()),
            p->GetSpeed(MOVE_RUN), ::Playerbot::BotMovement::NoPathStorm(p) ? 1 : 0, w.tx, w.ty, w.tz, reach,
            entry.ai->last_rule_fired() ? entry.ai->last_rule_fired() : "-");
        if (!on_mesh && wide)
        {
            Playerbot::BotMovement::SafeNearTeleport(p, wide_mesh.GetPositionX(), wide_mesh.GetPositionY(), wide_mesh.GetPositionZ() + 0.3f,
                                                     p->GetOrientation(), /*navmesh_point*/ true);
            TC_LOG_INFO("playerbot.v2", "[stuck_ledger] {} was off the navmesh, moved onto walkable ground", p->GetName());
        }
    });
    if (ledger_ms_.size() > 2048u)
        for (auto it = ledger_ms_.begin(); it != ledger_ms_.end();)
            it = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(it->first)) ? std::next(it) : ledger_ms_.erase(it);
}

// The variable pool (PoolBudget.h): temporary residents were created around a player; once one has been offline for VariableTtlHours,
// with no owner, group or queue tag and not flagged anything but temporary, the server's own character delete removes it. The core
// roster, a player's companions, saved or grouped bots and anything online are never selected, in the query and again live.
// One small batch per pass (the query runs off the world thread; only the few deletes run on it), logged with its cost.
void WorldBotSpawner::DriveRetire(uint32 now_ms)
{
    constexpr uint32 kRetirePassMs = 10u * 60u * 1000u;
    constexpr uint32 kRetirePerPass = 25u;
    if (variable_sweep_inflight_ || pool_count_ < 0 || query_inflight_)
        return;
    if (variable_sweep_ms_ && getMSTimeDiff(variable_sweep_ms_, now_ms) < kRetirePassMs)
        return;
    variable_sweep_ms_ = now_ms ? now_ms : 1u;
    variable_sweep_inflight_ = true;
    uint32 const ttl_h = cfg_.variable_ttl_h;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT pv.character_guid_low, c.account, c.name, c.online, "
        "(pv.owner_account_id <> 0 OR pv.owner_player_guid <> 0), "
        "EXISTS (SELECT 1 FROM `group_member` gm WHERE gm.memberGuid = c.guid), "
        "(pv.jit_for_queue IS NOT NULL AND pv.jit_for_queue <> ''), "
        "GREATEST(UNIX_TIMESTAMP() - c.logout_time, UNIX_TIMESTAMP() - UNIX_TIMESTAMP(pv.spawned_at)) "
        "FROM playerbot_v2_character pv JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE pv.variable_pool = 1 AND c.online = 0 AND pv.spawned_at < (NOW() - INTERVAL {} HOUR) "
        "AND c.logout_time < (UNIX_TIMESTAMP() - {}) ORDER BY c.logout_time LIMIT {}",
        ttl_h, uint64(ttl_h) * 3600u, kRetirePerPass).c_str())
        .WithCallback([this, ttl_h](QueryResult res)
    {
        variable_sweep_inflight_ = false;
        if (!res)
            return;
        uint32 const t0 = getMSTime();
        uint32 retired = 0;
        do
        {
            Field* f = res->Fetch();
            uint64 const guid = f[0].GetUInt64();
            RetireFacts facts;
            facts.variable = true;   // the query selects flagged rows only
            facts.online = f[3].GetUInt8() != 0 || pending_.contains(guid) || seeding_.contains(guid) || placed_.contains(guid);
            facts.owned = f[4].GetInt64() != 0;
            facts.grouped = f[5].GetInt64() != 0;
            facts.queue_tagged = f[6].GetInt64() != 0;
            facts.offline_seconds = f[7].GetInt64();
            if (!CanRetire(facts, int64(ttl_h) * 3600))
                continue;
            if (owner_.RetireVariableBot(guid, f[1].GetUInt32(), f[2].GetString()))
            {
                ++retired;
                --variable_count_;
            }
        } while (res->NextRow());
        if (retired)
        {
            n_retired_ += retired;
            TC_LOG_INFO("playerbot.v2", "[world] variable pool: {} temporary resident(s) deleted after {} h unused in {} ms (now {}/{}, {} deleted, {} created since start)",
                        retired, ttl_h, getMSTimeDiff(t0, getMSTime()), variable_count_, cfg_.variable_max, n_retired_, n_created_variable_);
        }
    }));
}

// The travel soak (Playerbot.World.TravelSoak = 1, off by default): a few ambient bots at a time are each sent to a random town, flight master,
// quest hub or dock of the travel graph, on their own map or another, with the quest travel executor (idle:soak_travel). Every leg is logged
// when it ends - arrived, timed out where it stood, or no route - and a summary groups the legs that never arrived by the 50 y cell they
// stopped in, so an overnight run answers "which legs never arrive, and where" (roadmap Track 4). Soak runners are not despawned.
// What the bot of a finished soak leg was doing when it ended, for the [travel_soak] line: the rule that last moved it, its zone,
// whether it rides a transport, and the last five things its logic asked the API for with the answer (Ok / Locked ...). A leg that
// ends far from the goal says why in the same line, so the log alone names the cause.
static std::string SoakBotFacts(uint64 guid)
{
    Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
    if (!p || !p->IsInWorld())
        return " | bot gone";
    char const* last_rule = "-";
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (uint64(id) == guid && e.ai && e.ai->last_rule_fired())
            last_rule = e.ai->last_rule_fired();
    });
    static constexpr char const* kResult[] = { "Ok", "NotReady", "OutOfRange", "InvalidTarget", "NotEnoughResource", "NotKnown", "ServerRefused", "InventoryFull", "Locked", "Other" };
    std::vector<IntentHistoryEntry> hist;
    Services::Registry().for_each_intent_history(BotId(guid), [&](size_t, IntentHistoryEntry const& h) { hist.push_back(h); });
    std::string tail;
    for (size_t i = hist.size() > 5u ? hist.size() - 5u : 0u; i < hist.size(); ++i)
    {
        IntentHistoryEntry const& h = hist[i];
        tail += Trinity::StringFormat(" {}:{}", ::Playerbot::IntentKindName(h.intent_kind), h.result < std::size(kResult) ? kResult[h.result] : "?");
        if (h.x != 0.0f || h.y != 0.0f)
            tail += Trinity::StringFormat("@({:.0f},{:.0f},{:.0f})", h.x, h.y, h.z);
    }
    return Trinity::StringFormat(" | last_rule={} zone={} on_transport={} moving={} sitting={} intents:{}", last_rule, p->GetZoneId(), p->GetTransport() ? 1 : 0,
                                 p->isMoving() ? 1 : 0, p->GetStandState() != UNIT_STAND_STATE_STAND ? 1 : 0, tail.empty() ? " -" : tail);
}

void WorldBotSpawner::DriveTravelSoak(uint32 now_ms)
{
    using namespace ::Playerbot::V2::Travel;
    SoakBoard& board = SoakBoard::Instance();
    for (SoakOutcome const& o : board.Drain())
    {
        TC_LOG_INFO("playerbot.v2",
            "[travel_soak] leg {} {} bot={} from map {} ({:.0f},{:.0f},{:.0f}) to '{}' kind {} map {} ({:.0f},{:.0f},{:.0f}) after {} s; ended map {} ({:.0f},{:.0f},{:.0f}) cell {}; plan leg {}/{}{}",
            o.goal.id, SoakResultName(o.result), o.guid, o.goal.start_map, o.goal.sx, o.goal.sy, o.goal.sz, o.goal.name, uint32(o.goal.node_kind), o.goal.map,
            o.goal.x, o.goal.y, o.goal.z, o.duration_ms / 1000u, o.end_map, o.ex, o.ey, o.ez, SoakCell(o.end_map, o.ex, o.ey), o.leg_index, o.plan_legs,
            o.result == SoakResult::Arrived ? std::string() : SoakBotFacts(o.guid));
    }
    if (!cfg_.travel_soak && !board.Active())
        return;
    for (uint64 const guid : board.ActiveGuids())
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
        if (!p || !p->IsInWorld())
            board.Cancel(guid);
    }
    if (getMSTimeDiff(soak_summary_ms_, now_ms) >= 10u * 60u * 1000u)
    {
        soak_summary_ms_ = now_ms ? now_ms : 1u;
        uint32 const arrived = board.Total(SoakResult::Arrived), timeout = board.Total(SoakResult::TimedOut), none = board.Total(SoakResult::NoRoute);
        if (arrived + timeout + none)
        {
            std::string cells;
            for (auto const& [cell, n] : board.TopStops(10))
                cells += Trinity::StringFormat(" {}x{}", cell, n);
            TC_LOG_INFO("playerbot.v2", "[travel_soak_summary] legs {}: arrived {}, timeout {}, no_route {}; stopped most in cells (map/x/y of 50 y):{}",
                        arrived + timeout + none, arrived, timeout, none, cells.empty() ? std::string(" -") : cells);
        }
    }
    if (!cfg_.travel_soak || board.Active() >= cfg_.travel_soak_bots)
        return;
    if (soak_ms_ && getMSTimeDiff(soak_ms_, now_ms) < 5000u)
        return;
    soak_ms_ = now_ms ? now_ms : 1u;

    // One bot per pass: an ambient world bot that is free to travel (no group, battleground, instance, combat or pending login).
    std::vector<Player*> free_bots;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !owner_.IsAmbientBot(id, p)) return;
        if (p->GetGroup() || p->InBattleground() || p->InBattlegroundQueue() || p->IsInCombat() || p->IsBeingTeleported()) return;
        if (Map* m = p->GetMap(); !m || m->IsDungeon() || m->IsBattlegroundOrArena()) return;
        if (pending_.contains(uint64(id)) || seeding_.contains(uint64(id)) || leaving_.contains(uint64(id)) || board.Has(uint64(id))) return;
        if (!owner_.SetupDone(uint64(id))) return;
        free_bots.push_back(p);
    });
    if (free_bots.empty())
        return;
    Player* bot = free_bots[urand(0, uint32(free_bots.size() - 1))];

    UnifiedTravelGraph& graph = Services::TravelGraph();
    size_t const nodes = graph.GetNodeCount();
    if (!nodes)
        return;
    GraphNode const* dest = nullptr;
    for (int tries = 0; tries < 60 && !dest; ++tries)
    {
        GraphNode const* n = graph.GetNode(NodeId(urand(0, uint32(nodes - 1))));
        if (!n) continue;
        if (n->kind != NodeKind::FlightMaster && n->kind != NodeKind::Capital && n->kind != NodeKind::QuestHub && n->kind != NodeKind::Dock) continue;
        if (n->faction != 0 && n->faction != bot->GetTeam()) continue;
        if (n->map_id == bot->GetMapId() && Dist2D(n->x, n->y, bot->GetPositionX(), bot->GetPositionY()) < 300.0f) continue;
        dest = n;
    }
    if (!dest)
        return;
    SoakGoal g;
    g.id = soak_next_id_++;
    g.map = dest->map_id; g.x = dest->x; g.y = dest->y; g.z = dest->z;
    g.name = dest->name.empty() ? std::string("-") : dest->name;
    g.node_kind = uint8(dest->kind);
    g.start_map = bot->GetMapId(); g.sx = bot->GetPositionX(); g.sy = bot->GetPositionY(); g.sz = bot->GetPositionZ();
    g.started_ms = now_ms ? now_ms : 1u;
    g.timeout_ms = cfg_.travel_soak_leg_s * 1000u;
    board.Assign(bot->GetGUID().GetCounter(), g);
}

void WorldBotSpawner::DriveGraduation(uint32 now_ms)
{
    // Every few seconds, at most two bots per pass: a bot reaching level 10 on the isle is moved the way a new level-10 bot is
    // placed, once its own setup is done (the pipeline may still be running its steps).
    constexpr uint32 kGraduationPassMs = 5000u;
    constexpr uint32 kGraduationPerPass = 2u;
    if (last_grad_ms_ && getMSTimeDiff(last_grad_ms_, now_ms) < kGraduationPassMs)
        return;
    last_grad_ms_ = now_ms;
    uint32 moved = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (moved >= kGraduationPerPass) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || !p->GetMap() || !Skyborne::IsSkyborneRace(p->GetRace())) return;
        bool const owned = !owner_.IsAmbientBot(id, p);   // a player's companion or alt is never ours to move
        bool const in_transit = p->IsBeingTeleported() || p->IsInFlight() || p->GetTransport() != nullptr;
        if (!Skyborne::ShouldGraduate(p->GetMapId(), p->GetLevel(), owned, p->GetGroup() != nullptr, p->IsInCombat(), in_transit)) return;
        if (!owner_.SetupDone(uint64(id)) || pending_.contains(uint64(id)) || seeding_.contains(uint64(id))) return;
        if (!owner_.GraduateSkyborne(p)) return;
        ++moved;
        ++n_graduated_;
        placed_[uint64(id)] = Placed{ now_ms };   // freshly placed: the despawn pass leaves it alone for the grace
        TC_LOG_INFO("playerbot.v2", "[world] skyborne graduate {} ({}) L{}: left Zephras Isle for map {} zone {}", p->GetName(), uint64(id),
                    uint32(p->GetLevel()), p->GetMapId(), p->GetZoneId());
    });
}

void WorldBotSpawner::DriveSeed(uint32 now_ms)
{
    // Finish what is in flight first: once a seeded bot's setup is done and it has stood still for a few seconds at its
    // zone hub (the pipeline's own teleport), log it out. Its position is saved, so it sleeps right there.
    constexpr uint32 kSeedSettleMs = 8000u, kSeedTimeoutMs = 10u * 60u * 1000u;
    for (auto it = seeding_.begin(); it != seeding_.end();)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(it->first);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        Seeding& sd = it->second;
        bool const timed_out = getMSTimeDiff(sd.started_ms, now_ms) >= kSeedTimeoutMs;
        if (!bot || timed_out)
        {
            if (bot)
                Services::SessionMgr().LogoutBot(g);   // never finished setup: do not keep it awake forever
            it = seeding_.erase(it);
            continue;
        }
        if (bot->IsInWorld() && !bot->IsBeingTeleported() && !bot->IsInCombat() && !bot->GetGroup() && owner_.SetupDone(it->first))
        {
            if (!sd.setup_done_ms)
                sd.setup_done_ms = now_ms ? now_ms : 1u;
            else if (getMSTimeDiff(sd.setup_done_ms, now_ms) >= kSeedSettleMs)
            {
                if (Services::SessionMgr().LogoutBot(g))
                {
                    ++n_seeded_;
                    it = seeding_.erase(it);
                    continue;
                }
            }
        }
        ++it;
    }

    if (!cfg_.seed_roster || pool_count_ < 0 || rss_capped_)
        return;
    // The seeder only builds the standing roster (core). Temporary residents are created on demand around a player (FinishSpawn).
    // Once the census is read, what the roster lacks is decided per (faction, level bracket) against the bracket cap, not by the total: a
    // roster that grew past PoolMax in the old days (brackets 10+ over their cap) would otherwise never get its short level 1-9 bracket.
    if (!pool_counts_ms_ && pool_count_ >= int64(cfg_.pool_max))
        return;
    if (seeding_.size() >= cfg_.seed_in_flight || ambient_online_ + pending_.size() + seeding_.size() >= cfg_.max_bots)
        return;
    // One new resident per tick: the setup pipeline (gear, talents, spells, teleport) is the cost, not the creation.
    // The census decides which faction and level bracket the roster lacks most; until it is read the seeder waits.
    if (!pool_counts_ms_)
        return;
    // Every (faction, level bracket) has a target of its own, 1-9 included; the roster is filled where it lacks most and never past a
    // bracket's cap, so the factions stay level and the 60s cannot crowd out the low levels.
    SeedChoice const want = PickSeed(pool_counts_, cfg_.pool_horde_pct, double(urand(0, 999999)) / 1000000.0, PoolBracketCap(cfg_.pool_max));
    if (want.deficit <= 0)
        return;   // every (faction, bracket) is at its target
    bool const alliance = want.alliance;
    uint8 const level = uint8(urand(PoolBracketLo(want.bracket), std::min<uint32>(cfg_.max_level, PoolBracketHi(want.bracket))));
    if (uint64 const guid = owner_.SpawnAmbientBot(level, alliance, uint8(0)))
    {
        seeding_[guid] = Seeding{ now_ms ? now_ms : 1u, 0 };
        ++pool_count_;
        PoolAdd(pool_counts_, alliance, level);
    }
    else
        ++n_spawn_failed_;
}

void WorldBotSpawner::DriveWake(uint32 now_ms, Anchor const& a, uint32 want)
{
    uint8 lo = 0, hi = 0;
    WindowFor(a, lo, hi);
    float const r = cfg_.wake_radius;
    bool const town = ::Playerbot::V2::World::IsVanillaCapitalZone(a.zone);
    // A rider: the sleepers on the road ahead of him wake, not the ones he rides away from. The box is centred ahead of him, at the
    // middle of the lead range, and the callback keeps only the sleepers in his corridor ahead (Fleet/MovingBubble.h).
    bool const riding = a.motion.moving && !town && !::Playerbot::V2::World::IsStartZone(a.zone);
    float lead_lo = 0.0f, lead_hi = 0.0f;
    float cx = a.x, cy = a.y;
    if (riding)
    {
        LeadRange(a.motion, cfg_.activity_radius, cfg_.moving, lead_lo, lead_hi);
        float const s = a.motion.Speed();
        float const shift = std::min((lead_lo + lead_hi) * 0.5f, r);
        cx += a.motion.vx / s * shift;
        cy += a.motion.vy / s * shift;
    }
    query_inflight_ = true;
    query_started_ms_ = now_ms;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT pv.character_guid_low, c.level, c.race, c.map, c.position_x, c.position_y, c.zone, c.position_z FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE c.online = 0 AND pv.distribution_level > 0 AND c.map = {} AND c.level BETWEEN {} AND {} "
        "AND c.position_x BETWEEN {:.1f} AND {:.1f} AND c.position_y BETWEEN {:.1f} AND {:.1f} {}{}"
        "ORDER BY RAND() LIMIT {}",
        a.map, uint32(lo), uint32(hi), cx - r, cx + r, cy - r, cy + r,
        SkyborneRaceClause(Skyborne::IsOnIsle(a.map)),
        town ? Trinity::StringFormat("AND c.zone = {} ", a.zone) : std::string(),   // a capital wakes its own zone only (WakeSelect.h)
        std::max<uint32>(want * 10u, 60u)).c_str())
        .WithCallback([this, a, want, town, riding, lead_lo, lead_hi](QueryResult res)
    {
        query_inflight_ = false;
        uint32 woken = 0;
        uint32 const now = now_ms_;
        if (res)
        {
            // Rule in Fleet/WakeSelect.h (tested): the distance window, the faction roll and, for a capital, the capital's own zone.
            WakeRule rule;
            rule.min_dist = 60.0f;
            rule.max_dist = riding ? std::max(cfg_.wake_radius, lead_hi) : cfg_.wake_radius;
            rule.capital = town;
            rule.anchor_zone = a.zone;
            std::vector<WakeCandidate> cands;
            do
            {
                Field* f = res->Fetch();
                uint64 const guid = f[0].GetUInt64();
                if (pending_.contains(guid) || seeding_.contains(guid)) continue;
                if (Services::Owners().GetOwner(guid).account_id != 0) continue;   // a companion is never a resident
                if (::Playerbot::Pool::IsScenarioBot(guid)) continue;   // a scenario test character is never a resident (Tier 3 test gear)
                if (ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid))) continue;
                // A bot of the wrong race saved in a start zone (older placements) is not woken there.
                if (!::Playerbot::V2::World::RaceFitsStartZone(f[2].GetUInt8(), f[1].GetUInt8(), f[6].GetUInt32())) continue;
                if (riding)
                {
                    float along = 0.0f, side = 0.0f;
                    AlongSide(a.x, a.y, a.motion.vx, a.motion.vy, f[4].GetFloat(), f[5].GetFloat(), along, side);
                    if (along < lead_lo * 0.5f || side > cfg_.moving.corridor_yards) continue;   // behind him or off his road
                }
                WakeCandidate c;
                c.guid = guid;
                c.dist = Dist2D(f[4].GetFloat(), f[5].GetFloat(), a.x, a.y);
                c.zone = f[6].GetUInt32();
                if (Player* ap = ObjectAccessor::FindConnectedPlayer(a.guid); ap && ap->GetMap())
                    c.wet = (ap->GetMap()->GetLiquidStatus(ap->GetPhaseShift(), f[4].GetFloat(), f[5].GetFloat(), f[7].GetFloat() + 0.05f, {})
                             & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) != 0;
                c.faction_ok = IsAllianceRace(f[2].GetUInt8()) == BotIsAllianceInZone(a.zone, a.alliance, urand(0, 99), FriendlyPctFor(cfg_.mix, a.zone));
                cands.push_back(c);
            } while (res->NextRow());
            // Never more than the target still lacks, and never past the global cap.
            uint32 const room = cfg_.max_bots > uint32(ambient_online_ + pending_.size())
                ? cfg_.max_bots - uint32(ambient_online_ + pending_.size()) : 0u;
            for (uint64 const guid : SelectWakeShared(cands, rule, std::min(want, room), AwakeByZone(a)))
            {
                auto lr = Services::SessionMgr().LoginBot(ObjectGuid::Create<HighGuid::Player>(guid));
                if (!lr.ok) { ++n_login_failed_; continue; }
                pending_[guid] = Pending{ a.guid, now, /*global*/ true, /*wake*/ true };   // already standing at home: no placement
                ++n_logged_in_;
                ++n_woken_;
                ++woken;
                if (riding) ++n_moving_woken_;
            }
        }
        if (!woken)
        {
            ++n_wake_dry_;
            wake_dry_until_[a.guid.GetCounter()] = now + (riding ? 3000u : 15000u);   // a rider is somewhere else 3 s later
        }
    }));
}

void WorldBotSpawner::DriveGlobalSpawn(uint32 now_ms, std::vector<Anchor> const& anchors)
{
    if (!cfg_.global_fill || anchors.empty() || query_inflight_ || rss_capped_)
        return;
    // Keep room for the bubble around every player: the filler only uses the rest of the budget.
    uint32 const reserve = std::min<uint32>(uint32(anchors.size()) * cfg_.bots_per_player, cfg_.max_bots / 2);
    uint32 const cap = cfg_.max_bots - reserve;
    uint32 const used = ambient_online_ + uint32(pending_.size());
    if (used >= cap)
        return;
    uint32 const want = std::min(cfg_.global_per_tick, cap - used);

    Anchor g = anchors.front();   // faction reference for FriendlyPct only
    g.guid = ObjectGuid::Empty;
    std::vector<bool>  slots;
    std::vector<uint8> slot_levels;
    for (uint32 i = 0; i < want; ++i)
    {
        slots.push_back(urand(0, 99) < cfg_.friendly_pct ? g.alliance : !g.alliance);
        slot_levels.push_back(uint8(urand(cfg_.min_level, cfg_.max_level)));
    }
    query_inflight_ = true;
    query_started_ms_ = now_ms;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT pv.character_guid_low, c.level, c.race, c.map, c.position_x, c.position_y, c.position_z FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE c.level BETWEEN {} AND {} AND c.online = 0 AND pv.distribution_level > 0 {}"
        "ORDER BY RAND() LIMIT {}",
        cfg_.min_level, cfg_.max_level, SkyborneRaceClause(false), std::max<uint32>(want * 40u, 200u)).c_str())
        .WithCallback([this, g, slots, slot_levels](QueryResult res)
    {
        query_inflight_ = false;
        FinishSpawn(g, slots, slot_levels, std::move(res), /*global*/ true);
    }));
}

void WorldBotSpawner::FinishSpawn(Anchor const& a, std::vector<bool> const& slots, std::vector<uint8> const& slot_levels,
                                  QueryResult res, bool global)
{
    struct Row { uint64 guid; bool alliance; bool used; bool in_hub; };
    // Around a player in a start zone (Deathknell, Northshire...) the low-level residents are the races that start there.
    bool const start_zone = !global && ::Playerbot::V2::World::IsStartZone(a.zone);
    std::vector<Row> rows;
    uint32 skipped_pending = 0, skipped_owned = 0, skipped_online = 0;   // rows the fetch brought but this pass cannot use
    if (res)
    {
        do
        {
            Field* f = res->Fetch();
            uint64 const guid = f[0].GetUInt64();
            if (pending_.contains(guid)) { ++skipped_pending; continue; }
            // A player's companion (owner-bound, e.g. dismissed with `.bot dismiss`) is never an ambient pool bot: logging it
            // back in would bring it back following its owner.
            if (Services::Owners().GetOwner(guid).account_id != 0) { ++skipped_owned; continue; }
            // A scenario test character (the dps scorecard's Tier 3 raid, the test squads) is never a resident either.
            if (::Playerbot::Pool::IsScenarioBot(guid)) { ++skipped_owned; continue; }
            if (ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid))) { ++skipped_online; continue; }
            // World-wide filler: only pool bots that were saved inside a quest hub / town. Memory grows with the number of
            // distinct places that have a Player in them (grids, mmap tiles), so the filler is kept on shared hubs.
            // Hub rows are preferred; rows saved elsewhere are the fallback (stage B, 2026-10-07: a hub-only filter left
            // every slot without a match, the pool was full, and nothing could be created, so growth stopped at 235).
            bool in_hub = true;
            if (global && cfg_.filler_hub_only)
            {
                Position const pos(f[4].GetFloat(), f[5].GetFloat(), f[6].GetFloat());
                ::Playerbot::V2::Travel::QuestHub const* hub = Services::Hubs().GetQuestHubAtPosition(pos);
                in_hub = hub && hub->mapId == f[3].GetUInt32();
            }
            if (start_zone && !::Playerbot::V2::World::RaceFitsStartZone(f[2].GetUInt8(), f[1].GetUInt8(), a.zone)) continue;
            rows.push_back({ guid, IsAllianceRace(f[2].GetUInt8()), false, in_hub });
        } while (res->NextRow());
    }

    // Same clock as OnWorldTick (module uptime), NOT getMSTime(): mixing them made every pending bot look 120 s old.
    uint32 const now = now_ms_;
    for (size_t i = 0; i < slots.size(); ++i)
    {
        if (ambient_online_ + pending_.size() >= cfg_.max_bots)
            break;
        bool alliance = slots[i];
        // A start zone's residents are of that zone's own team (an enemy of the player standing there is not a resident of it).
        uint8 const zone_race = start_zone && slot_levels[i] <= ::Playerbot::V2::World::kStartZoneMaxLevel
            ? ::Playerbot::V2::World::RaceForStartZone(a.zone, a.guid.GetCounter() + i) : uint8(0);
        if (zone_race != 0)
            alliance = IsAllianceRace(zone_race);
        bool done = false;
        for (int pass = 0; pass < 2 && !done; ++pass)    // pass 0: rows saved in a hub, pass 1: any row
            for (Row& r : rows)
            {
                if (r.used || r.alliance != alliance || (pass == 0 && !r.in_hub)) continue;
                r.used = true;
                auto lr = Services::SessionMgr().LoginBot(ObjectGuid::Create<HighGuid::Player>(r.guid));
                if (lr.ok)
                {
                    pending_[r.guid] = Pending{ a.guid, now, global };
                    ++n_logged_in_;
                    done = true;
                    break;
                }
                ++n_login_failed_;
            }
        if (done) continue;

        // No pool bot of that faction at that level: create one (bounded by PoolMax).
        ++n_no_match_;
        // Around a Zephras anchor every new resident is Skyborne (the other races cannot leave their start for the isle's quests).
        // The island's residents are always temporary; anyone else goes to the core roster while it has room and, when it is full,
        // to the variable pool - so a new character meets bots of its own level whatever the core is made of (PoolBudget.h).
        uint8 const race = !global && Skyborne::IsOnIsle(a.map) ? Skyborne::RaceForTeam(alliance) : zone_race;
        PoolBudget budget{ pool_count_, int64(cfg_.pool_max), variable_count_, int64(cfg_.variable_max) };
        // A (faction, level bracket) that is already at its share of the cap goes to the temporary pool, so the standing roster stays
        // balanced however much one faction is played (the census is read only with the seeder on; without it there is no cap).
        bool const core_full_here = pool_counts_ms_ && !PoolHasRoom(pool_counts_, slot_levels[i], alliance, PoolBracketCap(cfg_.pool_max), cfg_.pool_horde_pct);
        // With a census the bracket cap decides whether the standing roster takes it (the roster total can be over PoolMax from before).
        if (pool_counts_ms_ && race == 0)
            budget = BalancedBudget(budget, !core_full_here);
        PoolSlot const slot = ChoosePoolSlot(budget, race != 0 || core_full_here);
        if (slot == PoolSlot::None)
        {
            ++n_pool_full_;
            continue;
        }
        if (uint64 const guid = owner_.SpawnAmbientBot(slot_levels[i], alliance, race))
        {
            pending_[guid] = Pending{ a.guid, now, global };
            ++n_created_;
            PoolCountCreated(budget, slot);
            pool_count_ = budget.core;
            variable_count_ = budget.variable;
            if (slot == PoolSlot::Variable)
            {
                CharacterDatabase.PExecute("UPDATE playerbot_v2_character SET variable_pool = 1 WHERE character_guid_low = {}", guid);
                ++n_created_variable_;
            }
            else
                PoolAdd(pool_counts_, alliance, slot_levels[i]);
        }
        else
            ++n_spawn_failed_;
    }

    // One line per 30 s while a pass came up short, so a silent "nothing happens" is never a mystery.
    if (!global && n_no_match_ + n_login_failed_ && getMSTimeDiff(last_why_ms_, now) >= 30000u)
    {
        last_why_ms_ = now;
        TC_LOG_INFO("playerbot.v2",
            "[world] spawn pass around anchor (zone {}, L{}): wanted {} slot(s) ({} Alliance), {} offline pool row(s) usable ({} Alliance) "
            "of those fetched (skipped: {} pending, {} companions, {} already online); "
            "totals no_match={} pool_full={} (core {}/{}, variable {}/{}) spawn_failed={} login_failed={}",
            a.zone, uint32(a.level), slots.size(), uint32(std::count(slots.begin(), slots.end(), true)), rows.size(),
            uint32(std::count_if(rows.begin(), rows.end(), [](Row const& r) { return r.alliance; })),
            skipped_pending, skipped_owned, skipped_online, n_no_match_, n_pool_full_, pool_count_,
            cfg_.pool_max, variable_count_, cfg_.variable_max, n_spawn_failed_, n_login_failed_);
    }
}

void WorldBotSpawner::OnWorldTick(uint32 now_ms)
{
    // Config reload also covers "enabled at runtime"; while disabled this costs one config read per 15 s.
    // A choice from the ForeverBots panel (SetPopulation) is read at once, not at the next 15 s reload.
    if (!last_cfg_ms_ || pop_changed_ || getMSTimeDiff(last_cfg_ms_, now_ms) >= kConfigReloadMs)
    {
        last_cfg_ms_ = now_ms;
        pop_changed_ = false;
        LoadConfig();
    }
    if (drain_requested_)
    {
        drain_requested_ = false;
        drain_started_ms_ = now_ms ? now_ms : 1u;
        drain_until_ms_ = drain_started_ms_ + kDrainMaxMs;
        if (!drain_until_ms_) drain_until_ms_ = 1u;
        TC_LOG_INFO("playerbot.v2", "[world] population set to {}: the world bots online log out, then the new numbers fill",
                    pop_level_ ? ::Playerbot::V2::Fleet::PopLevelWord(*pop_level_) : "the config files");
    }
    // The [mem] line runs whatever the population mode: the 2026-10-11 leak hunt had World bots off and saw nothing.
    if (!last_mem_ms_ || getMSTimeDiff(last_mem_ms_, now_ms) >= kMemReportMs)
    {
        last_mem_ms_ = now_ms;
        TC_LOG_INFO("playerbot.v2", "{}", MemReport(all_online_));
    }
    if (!cfg_.enable)
    {
        WorldLeash::Clear();
        return;
    }
    if (uint32(sConfigMgr->GetIntDefault("Playerbot.Population.TotalTarget", 0, true)) > 0)
    {
        if (!warned_exclusive_)
        {
            TC_LOG_WARN("playerbot.v2", "[World] Playerbot.World.Enable is set but Playerbot.Population.TotalTarget > 0: "
                        "the two population modes are exclusive, world spawner stays off.");
            warned_exclusive_ = true;
        }
        return;
    }

    queries_.ProcessReadyCallbacks();
    LoadGearResetPending();
    if (query_inflight_ && getMSTimeDiff(query_started_ms_, now_ms) >= kQueryTimeoutMs)
        query_inflight_ = false;   // a QueryCallback always fires; this only guards a lost one

    if (last_tick_ms_ && getMSTimeDiff(last_tick_ms_, now_ms) < cfg_.tick_ms)
        return;
    last_tick_ms_ = now_ms;
    now_ms_ = now_ms;
    if (!rss_read_ms_ || getMSTimeDiff(rss_read_ms_, now_ms) >= 5000u)
    {
        rss_read_ms_ = now_ms;
        rss_kb_ = ProcessRssKb();
        if (uint64 const cpu = ProcessCpuMs())
        {
            if (cpu_prev_at_ && now_ms > cpu_prev_at_)
                cpu_pct_ = 100.0 * double(cpu - cpu_prev_ms_) / double(now_ms - cpu_prev_at_);
            cpu_prev_ms_ = cpu;
            cpu_prev_at_ = now_ms;
        }
        bool const was = rss_capped_;
        rss_capped_ = cfg_.rss_budget_mb && rss_kb_ / 1024 > cfg_.rss_budget_mb;
        if (rss_capped_ != was)
            TC_LOG_INFO("playerbot.v2", "[world] RSS {} MB {} the budget of {} MB: {}", rss_kb_ / 1024, rss_capped_ ? "is over" : "is back under",
                        cfg_.rss_budget_mb, rss_capped_ ? "no new filler bots" : "filler resumes");
    }
    uint32 const t0 = getMSTime();

    if (pool_count_ < 0 && !pool_count_inflight_)
    {
        pool_count_inflight_ = true;
        queries_.AddCallback(CharacterDatabase.AsyncQuery(
            Trinity::StringFormat(
            "SELECT COUNT(*), CAST(COALESCE(SUM(pv.variable_pool = 1), 0) AS UNSIGNED), "
            "CAST(COALESCE(SUM(pv.variable_pool = 0 AND c.race IN ({}, {}) AND c.level < {}), 0) AS UNSIGNED) "
            "FROM playerbot_v2_character pv JOIN characters c ON c.guid = pv.character_guid_low "
            "WHERE pv.distribution_level > 0 AND (pv.jit_for_queue IS NULL OR pv.jit_for_queue = '')",
            uint32(Skyborne::kRaceAlliance), uint32(Skyborne::kRaceHorde), uint32(Skyborne::kGraduationLevel)).c_str())
            .WithCallback([this](QueryResult res)
        {
            // The core roster (PoolMax) is the standing level 10+ residents. Temporary residents (flagged) and the island
            // residents created before the flag existed are counted against VariableMax, so a core full of level 60 bots can
            // never keep a new character's surroundings empty.
            int64 const total = res ? int64((*res)[0].GetUInt64()) : 0;
            int64 const flagged = res ? int64((*res)[1].GetUInt64()) : 0;
            int64 const legacy_isle = res ? int64((*res)[2].GetUInt64()) : 0;
            variable_count_ = flagged + legacy_isle;
            pool_count_ = std::max<int64>(0, total - variable_count_);
            pool_count_inflight_ = false;
        }));
    }

    // The roster by faction and level bracket, read every two minutes (creations in between are added as they happen): the seeder
    // creates whatever is furthest below the target ratio (PoolBalance.h).
    if (cfg_.seed_roster && !pool_counts_inflight_ && (!pool_counts_ms_ || getMSTimeDiff(pool_counts_ms_, now_ms) >= 120000u))
    {
        pool_counts_inflight_ = true;
        queries_.AddCallback(CharacterDatabase.AsyncQuery(
            "SELECT c.race, c.level, COUNT(*) FROM playerbot_v2_character pv JOIN characters c ON c.guid = pv.character_guid_low "
            "WHERE pv.distribution_level > 0 AND (pv.jit_for_queue IS NULL OR pv.jit_for_queue = '') AND (pv.variable_pool IS NULL OR pv.variable_pool = 0) "
            "GROUP BY c.race, c.level")
            .WithCallback([this, now_ms](QueryResult res)
        {
            PoolCounts counts;
            if (res)
                do
                {
                    Field* f = res->Fetch();
                    ((IsAllianceRace(f[0].GetUInt8()) ? counts.alliance : counts.horde))[PoolBracketOf(f[1].GetUInt8())] += uint32(f[2].GetUInt64());
                } while (res->NextRow());
            pool_counts_ = counts;
            pool_counts_ms_ = now_ms ? now_ms : 1u;
            pool_counts_inflight_ = false;
            uint32 a = 0, h = 0;
            for (std::size_t b = 0; b < kPoolBrackets; ++b) { a += counts.alliance[b]; h += counts.horde[b]; }
            TC_LOG_INFO("playerbot.v2", "[world] roster census: Alliance {} Horde {} (target Horde {}%, cap {} per bracket); per bracket 1-9/10s/20s/30s/40s/50s/60 A/H: {}/{} {}/{} {}/{} {}/{} {}/{} {}/{} {}/{}",
                a, h, cfg_.pool_horde_pct, PoolBracketCap(cfg_.pool_max), counts.alliance[0], counts.horde[0], counts.alliance[1], counts.horde[1],
                counts.alliance[2], counts.horde[2], counts.alliance[3], counts.horde[3], counts.alliance[4], counts.horde[4], counts.alliance[5], counts.horde[5],
                counts.alliance[6], counts.horde[6]);
        }));
    }

    std::vector<Anchor> anchors;
    CollectAnchors(anchors);
    TrackMotion(now_ms, anchors);
    anchors_ = uint32(anchors.size());
    PublishStrangers();
    PublishParties(now_ms);

    // A player who went into an instance or jumped far (hearth, flight end) leaves a ghost anchor for ZoneGraceSec, so the
    // bots around the spot he left are not logged out and rebuilt on the way back.
    {
        std::unordered_set<uint64> seen;
        for (Anchor const& a : anchors)
        {
            uint64 const k = a.guid.GetCounter();
            seen.insert(k);
            auto it = prev_anchor_.find(k);
            if (it != prev_anchor_.end() && (it->second.map != a.map || Dist2D(it->second.x, it->second.y, a.x, a.y) > cfg_.despawn_radius))
            {
                // He jumped (teleport, hearth, flight end) and is still in the open world: the bots around the place he left are
                // not kept for the long grace, which blocked the new place from filling (the cap was full of them); a short one
                // covers a hop back, and the swap runs at a higher rate for a minute (SwapBurstMs).
                if (cfg_.jump_grace_ms)
                {
                    ghosts_.push_back({ it->second, now_ms + cfg_.jump_grace_ms });
                    ghosts_.back().a.motion = Motion{};
                }
                swap_burst_until_ms_ = now_ms + kSwapBurstMs;
                wake_dry_until_.clear();   // a dry wake pass at the old place says nothing about the new one
                TC_LOG_INFO("playerbot.v2", "[world] jump: player moved {:.0f} y (map {} -> {}): old bots kept {} s, swap burst for {} s",
                            it->second.map != a.map ? -1.0f : Dist2D(it->second.x, it->second.y, a.x, a.y), it->second.map, a.map,
                            cfg_.jump_grace_ms / 1000u, kSwapBurstMs / 1000u);
            }
            prev_anchor_[k] = a;
        }
        for (auto it = prev_anchor_.begin(); it != prev_anchor_.end();)
        {
            if (seen.count(it->first)) { ++it; continue; }
            // Gone from the open world (instance, battleground, logout): keep his bots for the grace.
            if (cfg_.zone_grace_ms)
            {
                ghosts_.push_back({ it->second, now_ms + cfg_.zone_grace_ms });
                ghosts_.back().a.motion = Motion{};
            }
            it = prev_anchor_.erase(it);
        }
        ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(),
            [&](Ghost const& g) { return int32(g.until_ms - now_ms) <= 0; }),
            ghosts_.end());
    }
    std::vector<Anchor> keep_anchors = anchors;   // despawn looks at ghosts too; spawning and placing do not
    for (Ghost const& g : ghosts_)
        keep_anchors.push_back(g.a);
    PlanPlaces(now_ms, anchors, keep_anchors);

    {
        std::vector<WorldLeash::Anchor> pub;
        pub.reserve(keep_anchors.size());
        for (Anchor const& a : keep_anchors)
            pub.push_back({ a.map, a.x, a.y });
        // Objectives farther than 85% of the despawn radius from every anchor are not pursued (0 = no anchors, no leash).
        WorldLeash::Publish(std::move(pub), cfg_.despawn_radius * 0.85f);
    }

    DrivePending(now_ms, anchors);
    DriveDespawn(now_ms, keep_anchors);
    // While a step down (or Off) still has world bots logging out, nothing new comes: a new bot would only be logged out again.
    if (drain_until_ms_)
        DriveDrain(now_ms, keep_anchors);
    else
    {
        DriveSpawn(now_ms, anchors);
        DriveSeed(now_ms);
    }
    DriveRetire(now_ms);
    DriveTravelSoak(now_ms);
    DriveGraduation(now_ms);
    DriveStranded(now_ms, anchors);
    if (!drain_until_ms_)
        DriveIdleSwap(now_ms, anchors);
    DriveStuckLedger(now_ms);
    DriveLeaving(now_ms, anchors);
    DriveCrowd(now_ms, anchors);
    DriveGateDuels(now_ms, anchors);
    DriveGatherings(now_ms, anchors);

    last_tick_cost_ms_ = getMSTimeDiff(t0, getMSTime());
    max_tick_cost_ms_ = std::max(max_tick_cost_ms_, last_tick_cost_ms_);
    tick_cost_sum_ms_ += last_tick_cost_ms_;
    ++tick_count_;
    // Baseline for the per-bot RAM figure: the last reading taken while no ambient bot was online.
    if (ambient_online_ == 0 && !baseline_rss_kb_)
        baseline_rss_kb_ = ProcessRssKb();   // first reading only: later ones include memory the process keeps after logouts

    // Every five minutes: which idle rules have won the bots' ticks so far (busiest 25), and the town-life and quest-spot rules by name
    // even at zero. A show that never starts is then visible as a rule that never fires, not a mystery.
    if (Services::Initialized() && (!last_rules_ms_ || getMSTimeDiff(last_rules_ms_, now_ms) >= 300000u))
    {
        last_rules_ms_ = now_ms;
        std::string row;
        for (auto const& [name, n] : Services::IdleRules().FireCensus(25, { "idle:inn_rest", "idle:mailbox", "idle:town_duel", "idle:campfire",
                                                                              "idle:quest_site_share", "idle:yell_lfg", "idle:rpg_visit",
                                                                              "idle:rpg_city_trip", "idle:rpg_ah_trip" }))
            row += Trinity::StringFormat(" {}={}", name, n);
        TC_LOG_INFO("playerbot.v2", "[rules] fired since start:{}", row);
    }

    if (!last_report_ms_ || getMSTimeDiff(last_report_ms_, now_ms) >= kReportMs)
    {
        last_report_ms_ = now_ms;
        TC_LOG_INFO("playerbot.v2", "{}", Report());
    }
}

} // namespace Playerbot::V2::Fleet
