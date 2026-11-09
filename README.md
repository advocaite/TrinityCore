# TrinityCore Forever (forever)

## This fork: WoW Forever / Classic 1.60.1 (client 1.60.1.70862)

> **Blizzlike only.** Every change here matches Blizzard's own game: fixes come from real game data (packet captures,
> live hotfixes, player caches, Wowhead and Questie data, VMaNGOS for the vanilla world) or fix the server itself
> (crashes, packets, pathing, scripts). No custom items, no custom content, and every rate and option defaults to
> Blizzard's behaviour. Anyone may copy and use it under the GPL-2.0.

> **Not an official TrinityCore branch.** This is a community fork. It is not made, supported or endorsed by the
> TrinityCore team or by Blizzard Entertainment, and it ships no game files: you need your own client. Please don't ask
> the TrinityCore team for help with it: questions and bug reports go to our [Discord](https://discord.gg/7kJH4YyvZf).

Branches:
- `forever`: the clean core (this branch).
- `forever-bots`: the same core plus the optional player bots module (`src/modules/PlayerbotV2`). Bots stay off until
  you turn them on (`Playerbot.Enable`, `Playerbot.World.Enable`).

The ready-to-play Windows repack is built from a tagged `forever-bots` commit; you choose bots on or off when you start it.

This fork was started by advocaite as the `forever` branch of TrinityCore and is now kept up to date here. Thanks to
advocaite and to everyone who sent fixes.

Branch `forever` runs TrinityCore master against the **WoW Forever client 1.60.1** (`_forever_\Wow.exe`, Battle.net
product `wowf`) with a vanilla world (converted from VMaNGOS) and the Skyborne / Zephras Isle content.

**Just want to play?** Use the Windows repack from the releases: extract, run `Setup.bat`, start the servers in the
Forever Launcher, press Play. No MySQL or compiling needed.

**Letting other people play on your server?** See [Hosting for other players](#hosting-for-other-players). A full
Linux (Ubuntu 24.04) setup, tested on a fresh cloud server, is in [doc/LinuxServerSetup.md](doc/LinuxServerSetup.md).

### Building from source

Build the core as usual (see [Install](#install)), then:

**1. Databases.** Download the Forever database release
[forever-tdb-2026-10-14](https://github.com/Karrz1/TrinityCore/releases/tag/forever-tdb-2026-10-14), unpack it and
put `TDB_full_world_forever_2026_10_14.sql` and `TDB_full_hotfixes_forever_2026_10_14.sql` in the worldserver folder
(the folder you start worldserver from). On the first start worldserver's auto-setup imports them and then applies
only the `sql/custom` files added after the release (`Updates.EnableDatabases = 15`); nothing else is needed. The
release is the world built from source (TrinityCore TDB 1210.26091, all TrinityCore updates and all of our
`sql/custom` files, including the vanilla world converted from VMaNGOS), so you skip that whole conversion.

Already have a database? Just pull and start worldserver: it applies the new files on top. Building from
TrinityCore's own TDB 1210.26091 instead still works (rename it or change `revision_data.h.in.cmake`) but takes much
longer and gives the same result.

**2. Config.** The Classic client needs these (everything else can stay default):

`worldserver.conf`
```
RealmID = 70                                  # the Classic client only accepts realm 70 (the auth SQL renames realm 1)
Expansion = 0
Network.SkipBuildAuthKeyCheck = 1
Network.EnterEncryptedModeRegionGroup = 8
```
`bnetserver.conf`
```
Realm.CfgContentSetID = 137
LoginREST.ExternalAddress = trinity.actual.battle.net
LoginREST.LocalAddress = trinity.actual.battle.net
CertificatesFile = "./<your cert chain>.pem"  # certificate for trinity.actual.battle.net, see below
PrivateKeyFile = "./<your key>.pem"
```
If worldserver stops right after `Realm running as realm ID 1`, `RealmID` is not 70.

`Network.EnterEncryptedModeRegionGroup` is not in `worldserver.conf.dist`; add it by hand. The two `LoginREST`
names are what the client is told to connect to, and bnetserver must be able to resolve them itself at startup, so
the **server PC also needs the hosts entry** from step 3 (else bnetserver stops with "Could not resolve
LoginREST.ExternalAddress"). On Linux, `contrib/linux/configure_server.sh` writes both configs from the `.dist` files.

**Game data (maps, vmaps, mmaps).** Run the tools built from this branch in the **World of Warcraft** folder (the one that
holds `.build.info` and `_forever_`). Since client 1.60.1.70790 the Forever client is its own CASC product **`wowf`**
(`_forever_\Wow.exe`; before that it was `wow_classic_beta` in `_classic_beta_\WowB.exe`). This branch's extractors use `wowf`
by default (TrinityCore's own default `wow` = retail fails with "Error opening casc storage / No locales detected"; for an
old beta install pass `-p wow_classic_beta`):
```
mapextractor
vmap4extractor
mkdir vmaps
vmap4assembler Buildings vmaps
mmaps_generator
```
Then move `dbc`, `gt`, `maps`, `vmaps` and `mmaps` into the worldserver's `DataDir`.

**3. Login.** The client logs in to `trinity.actual.battle.net` over TLS, so on the client PC:

- hosts file: `127.0.0.1 trinity.actual.battle.net` (or your server's IP)
- a certificate for `trinity.actual.battle.net` signed by a root CA that the client PC trusts. Create your **own** CA
  (restrict it with `nameConstraints = critical,permitted;DNS:actual.battle.net`) and never share its private key.
  The repack's `setup\Setup.ps1` shows the exact openssl commands.
- start the client with `repack/Play-WoWForever.ps1`, which sets the portal and writes the server's public key into the running client.

### Hosting for other players

Out of the box everything only works on the server PC itself. For other PCs:

**Windows repack:** run `setup\host-for-others.bat` on the server PC. It opens the servers to the network, sets the
realm addresses, adds a Windows Firewall rule and makes `ForeverFriendKit.zip`. Each player unzips the kit and runs
`join-server.bat` (trusts your server certificate, sets their hosts line, checks the ports); `leave-server.bat` undoes
it. For internet play, forward TCP 1119, 8081, 8085, 8095, 8105, 8115 in your router to the server PC.
`host-for-others.bat -Undo` makes the server local-only again.

**Built from source,** check all of these (each one alone stops remote players):

| | What | Why |
|---|---|---|
| 1 | `BindIP = "0.0.0.0"` in bnetserver.conf and every worldserver.conf | `127.0.0.1` (the repack default) accepts this PC only |
| 2 | `auth.realmlist.address` = your public IP (or LAN IP), `localAddress` = the server's LAN IP | the default `127.0.0.1` sends players to their own PC after login |
| 3 | server PC: hosts entry `<server ip> trinity.actual.battle.net` | bnetserver must resolve the LoginREST names to start |
| 4 | every player: hosts entry `<server ip> trinity.actual.battle.net` | the client logs in to that name |
| 5 | every player trusts the root CA that signed your server certificate (give them the CA **certificate**, never its key) | the client checks the certificate; a repack's Setup.bat makes a different CA on every PC |
| 6 | firewall / router: TCP 1119 (bnet), 8081 (LoginREST.Port) and every realm's WorldServerPort | the client needs all three kinds |
| 7 | `LoginREST.ExternalAddress` stays `trinity.actual.battle.net`, not the IP | the certificate is only valid for that name |

Never open MySQL (3306/3317), Remote Access (3443) or SOAP (7878) to the internet.

### Player bots

Want bots? Use the `forever-bots` branch: this core plus the optional `src/modules/PlayerbotV2` module.

[![Average time to resolve an issue](https://isitmaintained.com/badge/resolution/TrinityCore/TrinityCore.svg)](https://isitmaintained.com/project/TrinityCore/TrinityCore "Average time to resolve an issue") [![Percentage of issues still open](https://isitmaintained.com/badge/open/TrinityCore/TrinityCore.svg)](https://isitmaintained.com/project/TrinityCore/TrinityCore "Percentage of issues still open")

## Setup

* [Website](https://www.trinitycore.org)
* [Wiki](https://www.trinitycore.info)
* [Forums](https://talk.trinitycore.org/)
* [Discord](https://discord.gg/7kJH4YyvZf)
