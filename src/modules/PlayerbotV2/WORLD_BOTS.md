# World bots (ambient bots around real players)

Branch `playerbot-v2-world`, cut from `playerbot-v2-raid` 91236c23. Owner: the "world bots" thread. Nothing here has run in game yet.

## Two population modes (exclusive)

| | Shaper (existing) | World spawner (new, `Fleet/WorldBotSpawner`) |
|---|---|---|
| Switch | `Playerbot.Population.TotalTarget > 0` | `Playerbot.World.Enable = 1` (needs `TotalTarget = 0`) |
| Headcount | fixed, spread over level brackets, all over the world | per real player, within `ActivityRadius` |
| Placement | level-appropriate vanilla town (`ZonePicker`) | spawn ring around the player, out of sight when possible |
| Despawn | session rhythm, LRU overflow | no real player within `DespawnRadius` |
| Use it for | "1000 bots live in the world" load test | "bots are around me wherever I go" |

Both reuse the same pool characters, `BotCharacterFactory`, `BotSetupPipeline` (level, trainer spells, weapon skills, talents, gear, riding, then a first teleport to a leveling town), and the same bot bot logic (questing, travel, grinding, death and release rules).

## What the world spawner does

Every `TickMs` (3 s) on the world thread:

1. **Anchors**: each real (non-bot) player in the open world (not dungeon, BG, arena).
2. **Pending**: bots we logged in or created are placed in the spawn ring around their anchor as soon as their setup pipeline reports done (`BotPopulationManager::SetupDone`). Timeout 120 s.
3. **Despawn**: an ambient bot (distribution level > 0, not an owner's companion or alt) farther than `DespawnRadius` from every anchor on its map is logged out, unless it is in combat, grouped, in an instance/BG, queued, kick-protected, or placed less than `PlacedGraceSec` ago. With no real player online every ambient bot is therefore logged out. At most `DespawnPerTick` per run.
4. **Spawn**: round-robin over anchors with fewer than `BotsPerPlayer` ambient bots within `ActivityRadius` (pending ones count). Level range = the player's zone level range +-2 (`VanillaZoneLevelRange`), else player level -8..+4, clamped to `MinLevel`..`MaxLevel`. One async query for offline pool bots in that range; each slot (faction rolled by `FriendlyPct`) takes a matching offline bot, else creates a new character (`SpawnAmbientBot`) while the pool is under `PoolMax`. At most `SpawnPerTick` per run and `MaxBots` online in total.

`[world]` status line every 60 s at INFO (needs `Logger.playerbot.v2=3`): anchors, bots online/max, pending, counters, spawner tick ms (last, avg, max), process RSS, baseline RSS, MB per bot.

Manager edits (all additive, `Fleet/BotPopulationManager`): `SetupDone`, `IsAmbientBot`, `SpawnAmbientBot` (lends `SpawnNew` one unit of budget, so the shaper's own budget is untouched), a `world_spawner_` member and one call in `OnWorldTick`.

## Config (`conf/playerbot.conf.dist`, section "World bots around real players")

`Playerbot.World.Enable, MaxBots, BotsPerPlayer, ActivityRadius, DespawnRadius, SpawnRingMin, SpawnRingMax, TickMs, SpawnPerTick, DespawnPerTick, PlacedGraceSec, MinLevel, MaxLevel, FriendlyPct, PoolMax`. All are re-read every 15 s, so `Enable` can be flipped without a restart.

## Staged test plan (for the PC, batched with the other tests)

Needs `Playerbot.Population.TotalTarget = 0`, `Logger.playerbot.v2=3`. Watch the `[world]` line, the `TickPerf` lines and the worldserver working set (`Get-Process worldserver` in PowerShell, before any bots and after each stage).

| Stage | Settings | Pass criteria |
|---|---|---|
| 1 | Enable=1, MaxBots=25, BotsPerPlayer=25 | bots appear in a ring 60-220 y away within ~2 min, wander/quest, nothing stands in town, `[world] pending` drains to 0 |
| 2 | walk 1-2 km away | bots near the old spot log out after the grace period, new ones appear around you; no teleport loops |
| 3 | MaxBots=100, BotsPerPlayer=100, ActivityRadius=600, SpawnRingMax=500 | TickPerf total avg < 50 ms, max < 150 ms; client FPS unchanged (check `[netstat]` once trial has it) |
| 4 | MaxBots=250 then 500 | same; note working set per bot |
| 5 | 1000: either World with MaxBots=1000, BotsPerPlayer=1000, ActivityRadius=2500, SpawnRingMax=2000 (all around one player), or the shaper with `TotalTarget=1000` and `Enable=0` (spread over the continents) | record cost per bot; stop at the first stage that misses the criteria |

Raise `SpawnPerTick` for a faster ramp; the setup pipeline drains at most 10 bots per second regardless, so 1000 fresh characters take about 2-3 minutes of setup after creation, and creating 1000 new characters is slow on the DB. The first run is much cheaper once the pool exists (offline pool bots are logged in without setup).

## Cost estimate (NOT measured: from code comments and structure only)

- Per bot the world pays: a `Player` plus its headless `WorldSession`, `BotAI` state, one snapshot and one intent queue, and its share of Map update (visibility, bot logic, movement). RAM is probably a few MB per bot (guess: 2-4 MB), so 1000 bots roughly 2-4 GB on top of the server (31 GB machine). Loaded map and mmap tiles add to that and grow with how widely the bots spread, not with the bot count.
- CPU: bot bot logic runs on a worker pool with activity tiers (Combat 100 ms, Active 150 ms, Cruise 300 ms, Idle 500 ms, Hibernate 2 s). A solo questing bot is Cruise. The Map update for the bots stays on the world thread(s), which is the likely limit, together with the SMSG traffic when bots are near the player.
- A previous retail deployment of this module ran 2000 bots on a 16-core desktop CPU (a 5950X; the owner's PC has 16 threads, an RTX 5090 is irrelevant here), with a baseline of about a third of the CPU. Classic maps are lighter than retail, but the Forever core, the Classic rotations and the raid brain add cost of their own, so treat 200 and 500 as probable, 1000 as the stretch target, and let stage 5 settle it.
- Known multiplier: bots near the player cost network traffic (see the SMSG_PARTY_UPDATE flood earlier today). Only grouped bots send party updates, so ambient bots should not, but nearby ambient bots produce visibility/update packets for the client: watch FPS in stage 3 and 4.

## Known gaps and risks

- Quest and travel on the vanilla quest data are unverified (the conf says so): bots may stand around or walk far away, and then get logged out as orphans while the player stays put. If bots leave the radius too quickly, raise `ActivityRadius`/`DespawnRadius`, or add a leash rule.
- A new bot is first teleported to a leveling town by the setup pipeline, then to the ring: two teleports on a first spawn (cheap for pool bots).
- World PvP mix: the faction share is per zone (`Fleet/WorldFactionMix.h`, keys `ZoneMix`, `CapitalFriendlyPct`, `HomeFriendlyPct`, `BorderFriendlyPct`, `ContestedFriendlyPct`, all 100 by default (no enemy-faction bots); unlisted zones use `FriendlyPct`). Contested zones (Plaguelands, Stranglethorn, Hillsbrad, Arathi, ...) hold ~40% opposing faction drawn from the same level window as friendly bots.
- Roster balance: with `SeedRoster = 1` the seeder creates whichever (faction, level bracket 10s/50-60) is furthest below `PoolHordePct` (default 50) Horde (`Fleet/PoolBalance.h`); the census is logged as `[world] roster census` every two minutes. `PoolMax` must leave room for the characters a lopsided pool lacks.
- Hostile-faction bots (`FriendlyPct` < 100) are placed near the player; on a PvP realm they will fight. Set `FriendlyPct = 100` to avoid that.
- Bots below level 10 are not spawned (no navmesh in the starter zones), so a player in a starter zone sees none.
- The spawner does not yet read the player's party: bots already in the player's group are companions and are ignored by design.
- Not done: gear/level realism beyond the existing pipeline, a `.bot world` status command, per-player overrides, a persistent "home zone" for bots that are not despawned.

## Status (2026-10-06, paused)

- Built, compiled on Linux, never run: spawner, `.bot world`, `[world]` log line (bots, spawner tick ms, RAM per bot), conf keys (all off by default). Head cd6e4db7 plus this note.
- Main has the digest section for `[world]` and a commented stage-1 key block in `scripts/make-configs.ps1` (add923b9); both unrun (no PowerShell in the cloud).
- Vanilla quest/travel code was audited by reading only: hub database, travel graph (staffed flight masters, maps 0 and 1), portal/boat index and zone picker all branch on `IsVanillaRealm`. No gap found; real behaviour is the stage 1 check on the PC.
- Next, once the PC results exist: read the `[world]` lines for the cost per bot, then stages 2 to 5.

## 2026-10-07: battlegrounds with world bots

- `BotPopulationManager::AmbientFleetEnabled()` (TotalTarget > 0 or World.Enable) now gates the BG/arena seed and `BotQueueFiller::Fill`, which used to need TotalTarget > 0. So with World.Enable=1 a real player's BG queue is filled with JIT bots, and `Playerbot.Bg.AutoSeed.Matches` > 0 seeds bot-vs-bot matches.
- `Playerbot.Bg.AutoSeed.Types` (comma list of BattlemasterList ids) was added concurrently by the raid thread; its default for Forever is WSG 2, AB 3, AV 1, then the modern ids, with ids lacking a battleground_template row dropped. The merge kept the raid thread's version.
- Trial's ambient BG auto-join (Playerbot.Bg.AutoJoin*) can queue world bots: the spawner never moves or logs out a bot that is queued for or inside a battleground.
- BG combat belongs to the raid thread (WSG/AB/AV scripts). Open: whether a JIT roster of 10/15/40 per side gets set up in time (level 60 pipeline), and whether the retail-era `req.bracket = 7` is right for Classic brackets (the bracket is resolved by level 60).

## 2026-10-07: world-wide population (GlobalFill)

- `Playerbot.World.GlobalFill = 1` (with Enable = 1): the part of the `MaxBots` budget that the bubble around real players does not need (reserve = BotsPerPlayer x players, at most half) is filled with bots anywhere: offline pool bots of any level log in where they were saved, missing ones are created at a random level 10-60 and the setup pipeline sends them to a leveling town of that level. No player online = no filler (the existing "no anchor, log everyone out" rule).
- Despawn changes with it: a far bot is no longer logged out just for being far; only when a player's bubble still has a deficit and the budget is spent, farthest first (only bots whose setup is done).
- Cost control is the existing activity tiers: a bot away from every real player is Cruise while moving, Idle/Parked when still (PlayerbotV2.cpp tier classifier), so far bots are cheap. The `[world]` line now shows `far=` (bots beyond ActivityRadius) and `global_settled=`; watch `spawner_tick_ms`, `rss_mb`, `per_bot_mb` per stage.
- Stages (set in scripts/make-configs.ps1, commented): 300 / 750 / 1500 = MaxBots, BotsPerPlayer 50-60, PoolMax at least 1.2 x MaxBots. Ramp is GlobalPerTick per 3 s, so 1500 takes about 10 min and the setup pipeline (10 bots/s) is not the limit.

## 2026-10-07 (evening): stage A follow-ups

- Setup pipeline pass has a 30 ms budget; the spawner runs every 1 s (2 bubble + 3 filler logins, 5 logouts per pass).
- Snapshot cadence: Cruise tier (solo far bots) 300 -> 600 ms; Cruise/Idle/Parked builds are capped at 64 per world frame so a long frame no longer makes every bot due at once (295 builds = 370 ms in one tick). Combat/Active are never capped.
- Death hotspots (`Bot/DeathHotspots.h`): open-world creature kills of bots are noted per 100 y cell; 3 deaths in 20 min with a killer more than 2 levels above the bot make the cell dangerous for that bot, and `idle:pursue_quest_goal` blacklists an objective inside it for 20 min.
- Memory: RSS did not shrink after 231 logouts (7320 MB at 0 bots, 4229 before), so most of the 13 MB/bot is retained grid/mmap/heap, not per-bot state. Per-bot cost cannot be read from one run; compare stage B (750) with the first reading before 1500.

## 2026-10-10: Skyborne bots on Zephras Isle (Classic 1.60, map 2991)
Not run on the PC yet. Rules in `Fleet/SkyborneIsle.h` (tested: `ctest -L skyborne`).
- The Skyborne races (95 Alliance, 96 Horde) are in the ambient race roll with weight 4 (about 9% of the Alliance and 11% of the Horde bots). At level 10 and up they are placed in a vanilla leveling town like every other bot.
- Zephras Isle is an island with no exit in the server data (its portals lead to Valanaar and Rohashi Spires on the same map; the Skyborne portals of Stormwind and Dalaran connect those two cities). So the isle is handled apart: around a real player on the isle the spawner keeps Skyborne bots of level 1-9 (MinLevel does not apply there), the pipeline leaves a level 1-9 Skyborne at its start, they quest with the same-map starter logic of low-level bots, and `WorldBotSpawner::DriveGraduation` moves a bot that reaches level 10 on the isle to a leveling town with `BotSetupPipeline::Graduate` (the placement a new level-10 bot gets). The graduation is the only carried move; it is logged as `[world] skyborne graduate`.
- Seeding (`SeedRoster = 1`) makes `SkyborneSeedPct` (5) of the residents sleeping Skyborne level 1-9 at the isle start, so they wake when a player comes near.
- Unverified: whether the quest givers of the isle deal with both Skyborne races in `giver_team_mask`, whether map 2991 is a normal open-world map for the anchor test (not an instance map), mmap coverage of the whole isle (33 mmtiles for 72 map tiles), and customization options for races 95/96 in `BotCharacterFactory`.

- Area cap: no more than `AreaCapOutdoors` (14) world bots within `AreaRadius` (45 y) of one another outdoors, `AreaCapTown` (40) in a capital. Placement refuses a full spot, and every 10 s a pass moves up to three extras of an over-full crowd next to the nearest player (`[world] crowd:` line). `Fleet/AreaCap.h`.

- World life: `DayRhythm` scales the targets by the server's hour (60% at night, 125% in the evening, never over MaxBots); `NaturalMoves` makes bots arrive on foot out of sight and leave toward an inn before logging out; `TownLife` adds the inn rest, duels, campfires and dungeon LFG calls (`Bot/TownLife.h`, rules in `Bot/States/Rules/AmbientRules.cpp`). Each show takes only a few percent of the bots at a time, so none adds RAM; nothing here forms a bot-only dungeon group.
