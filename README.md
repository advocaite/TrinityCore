# ForeverCore

[![License: GPL-2.0](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](COPYING)
[![Discord](https://img.shields.io/badge/chat-Discord-5865F2.svg)](https://discord.gg/eJt62dDh54)

## Introduction

ForeverCore is a free, open source server for the **WoW Forever client (Classic 1.60.1, build 70862)**. It is written
in C++ and based on [TrinityCore](https://github.com/TrinityCore/TrinityCore), with a vanilla world converted from
[VMaNGOS](https://github.com/vmangos/core).

It is made for people who want to run their own Blizzlike server, learn how the game works, or help make it more
correct. Anyone may copy it, change it and share it under the GPL-2.0.

Branches:

| Branch | What it is |
|---|---|
| `forever` (default) | The clean core. |
| `forever-bots` | The same core plus the optional player bots module (`src/modules/PlayerbotV2`). Bots stay off until you turn them on. |

**Just want to play?** Get the ready-to-play Windows repack from our [Discord](https://discord.gg/eJt62dDh54). It is
built from a tagged `forever-bots` commit, and you choose bots on or off when you start it.

## Philosophy

- **Blizzlike only.** Every change matches Blizzard's own game. Fixes come from real game data (packet captures, live
  hotfixes, player caches, Wowhead and Questie data, VMaNGOS for the vanilla world) or fix the server itself (crashes,
  packets, pathing, scripts).
- **No custom content.** No custom items, quests, NPCs or rates. Every rate and option defaults to Blizzard's behaviour.
- **Every data change names its source.** A reviewer must be able to check where a number came from.
- **Optional extras stay optional.** Server owner shortcuts (open gates, no attunement and so on) exist as config
  switches that are off by default. Bigger additions live in modules, not in the core.
- **Kept up to date.** Each ForeverCore release is synced here as one commit, with the release notes in plain words.

## Based on, and credits

- [TrinityCore](https://github.com/TrinityCore/TrinityCore) (GPL-2.0), which goes back to MaNGOS. See [AUTHORS](AUTHORS).
- The `forever` branch of TrinityCore started by [advocaite](https://github.com/advocaite/TrinityCore). Thanks to
  advocaite for starting it.
- [VMaNGOS](https://github.com/vmangos/core) (GPL-2.0) for the vanilla world, numbers and encounter scripts.
- [CMaNGOS classic-db](https://github.com/cmangos/classic-db) (GPL-2.0) and [Questie](https://github.com/Questie/Questie)
  (GPL-3.0) data, used as sources for quests, spawns and drops.
- The AnoCore project (GPL-2.0), for Forever quest data and spawns.
- Everyone who sent fixes, bug reports and packet captures.

This is a community project. It is not made, supported or endorsed by the TrinityCore team or by Blizzard
Entertainment. It ships no game files: you need your own client. Please don't ask the TrinityCore team for help with
it; questions and bug reports go to our [Discord](https://discord.gg/eJt62dDh54) or the
[issues](https://github.com/Karrz1/TrinityCore/issues).

## Config switches

Everything below is in `worldserver.conf.dist`, with a longer description next to each option. The defaults are
Blizzlike. The server owner shortcuts are explained in [doc/OptionalServerOptions.md](doc/OptionalServerOptions.md).

| Option | Default | What it does |
|---|---|---|
| `Instance.IgnoreAttunement` | 0 | 1 = no attunement item or quest needed to enter an instance. |
| `Instance.AhnQirajGatesOpen` | 0 | 1 = the Gate of Ahn'Qiraj stands open, no war effort or Scarab Gong needed. |
| `Instance.MoltenCoreSkipRunes` | 0 | 1 = the Molten Core runes count as doused; Majordomo and Ragnaros are up from the start. |
| `Instance.MoltenCoreRagnarosReady` | 0 | 1 = Ragnaros can be fought at once, without Majordomo's talk and summon. |
| `Instance.MoltenCoreHoundsLinked` | 0 | 1 = Core Hounds rise again unless the whole pack dies together, as in 1.12. |
| `Instance.BindOnEntry`, `Instance.PersistRaidTrash` | 0 | 1 = raid lockout on entry; killed trash stays dead until the reset. |
| `Classic.ForeverCreatureHealth` | 0 | 1 = creature health from the curve fit to sniffed Forever creatures instead of the VMaNGOS curve. |
| `Classic.CreatureHealth.RaiseFromLevel`, `.RaiseAt60Pct`, `.Raids` | 20, 20, 0 | Forever's extra health for higher level creatures (+20% at level 60, raids not included). |
| `Classic.Respawn.Level1to10Pct` ... `Level31to60Pct` | 50, 70, 85, 100 | Forever's faster respawns in low level zones. All 100 = off. |
| `Classic.ForeverKillXp` | 0 | 1 = kill XP for lower level mobs from the Forever sniff formula. |
| `Classic.Shop.Enabled` | 0 | 1 = in-game store. 0 answers like the official server. |
| `Rate.*` | 1 | All rates are Blizzard's. |
| `AuctionHouseBot.Seller.Enabled`, `.Buyer.Enabled` | 0 | Auction house bot off. |

## Build

You need a C++20 compiler, CMake, MySQL 8 (or MariaDB), Boost and OpenSSL. TrinityCore's
[install guide](https://trinitycore.info/install) lists the exact versions for Windows, Linux and macOS.

```
git clone -b forever https://github.com/Karrz1/TrinityCore.git ForeverCore
cmake -S ForeverCore -B build -DSERVERS=1 -DSCRIPTS=static -DTOOLS=1
cmake --build build --config RelWithDebInfo
```

The Forever specific steps (database release, `worldserver.conf` and `bnetserver.conf` settings, map extraction for
the `wowf` client, login certificate, hosting for other players) are in [doc/ForeverSetup.md](doc/ForeverSetup.md).
A full Linux (Ubuntu 24.04) server setup is in [doc/LinuxServerSetup.md](doc/LinuxServerSetup.md).

## Modules

A module is a folder in `src/modules/` with its own `CMakeLists.txt`. It plugs into the core's script hooks without
changing core files, and with no module the server is exactly the plain core. How to write one:
[src/modules/README.md](src/modules/README.md).

### Adding the player bots module

The bots module lives on the `forever-bots` branch. Either clone that branch, or merge it into your own tree:

```
git clone -b forever-bots https://github.com/Karrz1/TrinityCore.git ForeverCore-bots
cmake -S ForeverCore-bots -B build -DSERVERS=1 -DSCRIPTS=static -DBUILD_PLAYERBOT_V2=1
cmake --build build --config RelWithDebInfo --target worldserver
```

`BUILD_PLAYERBOT_V2` is the only bot build flag. Bots stay off until you turn them on in `playerbot.conf`
(`Playerbot.Enable`, `Playerbot.World.Enable`). Extras such as world PvP bots and the optional bot chat program are off
by default. Database setup for the module is in `src/modules/PlayerbotV2/INSTALL.md` on that branch.

## Contributing

Bug reports, data fixes and code are all welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) first: in short, one
fix per pull request, and every data change names its source.

## Links

- [Discord](https://discord.gg/eJt62dDh54)
- [Issues](https://github.com/Karrz1/TrinityCore/issues)
- [Releases](https://github.com/Karrz1/TrinityCore/releases) (source releases and the Forever world database)
- [TrinityCore](https://github.com/TrinityCore/TrinityCore), the upstream project

## License

ForeverCore is free software under the [GNU General Public License v2.0](COPYING), like TrinityCore. Libraries under
`dep/` keep their own licenses. World of Warcraft and its data belong to Blizzard Entertainment and are not included.
