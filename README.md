# ForeverCore

A server for the **WoW Forever client (Classic 1.60.1, build 70862)**, based on TrinityCore.

- Blizzlike: it plays like Blizzard's realms. Fixes come from real game data or fix the server itself.
- No custom items, quests or rates. Every option starts at Blizzard's setting.
- Free to copy, change and share under the GPL-2.0.

**Want to play?** Download the Windows repack from the
[latest release](https://github.com/Karrz1/Karrz-playerbots-wow-forever/releases/latest) (the
`WoW-Forever-Bots-<version>.zip` file). You turn bots on or off when you start it. The launcher updates itself from
[its own releases](https://github.com/Karrz1/foreverlauncher-releases/releases).

**Need help?** Ask on our [Discord](https://discord.gg/eJt62dDh54) or open an [issue](https://github.com/Karrz1/TrinityCore/issues).

## Branches

- `forever`: the core (this branch).
- `forever-bots`: the same core plus the optional player bots. Bots stay off until you turn them on.

The repack is built from a tagged `forever-bots` commit.

## Switches

All in `worldserver.conf`. Leave them at 0 for the normal Blizzard game, or turn on what you want for your own server.
More on each one: [doc/OptionalServerOptions.md](doc/OptionalServerOptions.md) and the notes in `worldserver.conf.dist`.

| Switch | Normal | Turned on |
|---|---|---|
| `Instance.IgnoreAttunement` | 0 | No attunement quest or item needed for dungeons and raids |
| `Instance.AhnQirajGatesOpen` | 0 | The Ahn'Qiraj gates stand open |
| `Instance.MoltenCoreSkipRunes` | 0 | Molten Core runes count as done; Majordomo and Ragnaros are up from the start |
| `Instance.MoltenCoreRagnarosReady` | 0 | Ragnaros can be fought at once, no Majordomo talk |
| `Instance.MoltenCoreHoundsLinked` | 0 | Core Hound packs must die together, as in 1.12 |
| `Instance.BindOnEntry`, `Instance.PersistRaidTrash` | 0 | Raid lock when you walk in; killed trash stays dead until the reset |
| `Classic.ForeverCreatureHealth` | 0 | Creature health from sniffed Forever creatures instead of the vanilla numbers |
| `Classic.ForeverKillXp` | 0 | Kill XP for lower level mobs from the Forever sniff formula |
| `Classic.Shop.Enabled` | 0 | In-game store |

These follow Forever's own changes and are on by default:

| Switch | Default | What it does |
|---|---|---|
| `Classic.Respawn.Level1to10Pct` ... `Level31to60Pct` | 50, 70, 85, 100 | Faster respawns in low level zones. All 100 = off |
| `Classic.CreatureHealth.RaiseFromLevel`, `.RaiseAt60Pct`, `.Raids` | 20, 20, 0 | More health for higher level creatures, up to +20% at 60, not in raids |

All `Rate.*` settings are 1, and the auction house bot is off.

## Build

You need a C++20 compiler, CMake, MySQL 8, Boost and OpenSSL (versions: TrinityCore's
[install guide](https://trinitycore.info/install)).

```
git clone -b forever https://github.com/Karrz1/TrinityCore.git ForeverCore
cmake -S ForeverCore -B build -DSERVERS=1 -DSCRIPTS=static -DTOOLS=1
cmake --build build --config RelWithDebInfo
```

Then follow [doc/ForeverSetup.md](doc/ForeverSetup.md): the database release, the config lines the client needs, map
extraction, the login certificate, and how to let other people play. For a Linux server see
[doc/LinuxServerSetup.md](doc/LinuxServerSetup.md).

## Bots

The bots are a module in `src/modules/PlayerbotV2`, on the `forever-bots` branch:

```
git clone -b forever-bots https://github.com/Karrz1/TrinityCore.git ForeverCore-bots
cmake -S ForeverCore-bots -B build -DSERVERS=1 -DSCRIPTS=static -DBUILD_PLAYERBOT_V2=1
cmake --build build --config RelWithDebInfo --target worldserver
```

Turn them on in `playerbot.conf` (`Playerbot.Enable`, `Playerbot.World.Enable`). World PvP bots and the bot chat
program are off by default. Database setup for the bots: `src/modules/PlayerbotV2/INSTALL.md` on that branch.
Other modules go in `src/modules/` too, see [src/modules/README.md](src/modules/README.md).

## Fixes welcome

Send a pull request against `forever`, one fix per pull request, and say where the data comes from (sniff, Wowhead,
Questie, video). Bug reports go in the [issues](https://github.com/Karrz1/TrinityCore/issues).

## Thanks

- [TrinityCore](https://github.com/TrinityCore/TrinityCore) and everyone in [AUTHORS](AUTHORS).
- advocaite, who started the `forever` branch.
- [VMaNGOS](https://github.com/vmangos/core) for the vanilla world, [CMaNGOS classic-db](https://github.com/cmangos/classic-db)
  and [Questie](https://github.com/Questie/Questie) for quest, spawn and drop data.
- The AnoCore project, for Forever quest data and spawns.
- Everyone who sent fixes, bug reports and packet captures.

Not made or supported by the TrinityCore team or by Blizzard Entertainment, so please don't ask them about it. No game
files are included: you need your own client. GPL-2.0, see [COPYING](COPYING).
