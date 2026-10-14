# ![logo](https://community.trinitycore.org/public/style_images/1_trinitycore.png) TrinityCore (master)

## This fork: WoW Classic beta 1.60.1.70338 ("Forever")

Branch `forever` runs TrinityCore master against the **WoW Classic beta client 1.60.1.70338** (`_classic_beta_\WowB.exe`)
with a vanilla world (converted from VMaNGOS) and the Skyborne / Zephras Isle content.

**Just want to play?** Use the Windows repack from the releases: extract, run `Setup.bat`, start the servers in the
Forever Launcher, press Play. No MySQL or compiling needed.

**Letting other people play on your server?** See [Hosting for other players](#hosting-for-other-players). A full
Linux (Ubuntu 24.04) setup, tested on a fresh cloud server, is in [doc/LinuxServerSetup.md](doc/LinuxServerSetup.md).

### Building from source

Build the core as usual (see [Install](#install)), then:

**1. Databases.** Download the Forever database release
[forever-tdb-2026-10-14](https://github.com/advocaite/TrinityCore/releases/tag/forever-tdb-2026-10-14), unpack it and
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
holds `.build.info` and `_classic_beta_`). The Forever client is the CASC product **`wow_classic_beta`**; this branch's
extractors use it by default (TrinityCore's own default `wow` = retail fails with "Error opening casc storage / No locales
detected"; on older builds of the tools pass `-p wow_classic_beta`):
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
- start the client with the [Forever Launcher](https://github.com/advocaite/foreverlauncher-releases/releases), which sets the portal and
  the server's public key.

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

[![Average time to resolve an issue](https://isitmaintained.com/badge/resolution/TrinityCore/TrinityCore.svg)](https://isitmaintained.com/project/TrinityCore/TrinityCore "Average time to resolve an issue") [![Percentage of issues still open](https://isitmaintained.com/badge/open/TrinityCore/TrinityCore.svg)](https://isitmaintained.com/project/TrinityCore/TrinityCore "Percentage of issues still open")

--------------


* [Build Status](#build-status)
* [Introduction](#introduction)
* [Requirements](#requirements)
* [Install](#install)
* [Reporting issues](#reporting-issues)
* [Submitting fixes](#submitting-fixes)
* [Copyright](#copyright)
* [Authors &amp; Contributors](#authors--contributors)
* [Links](#links)



## Build Status

master | 3.3.5 | cata_classic
:------------: | :------------: | :------------:
[![master Build Status](https://circleci.com/gh/TrinityCore/TrinityCore/tree/master.svg?style=shield)](https://circleci.com/gh/TrinityCore/TrinityCore/tree/master) | [![3.3.5 Build Status](https://circleci.com/gh/TrinityCore/TrinityCore/tree/3.3.5.svg?style=shield)](https://circleci.com/gh/TrinityCore/TrinityCore/tree/3.3.5) | [![cata_classic Build Status](https://circleci.com/gh/TrinityCore/TrinityCore/tree/cata_classic.svg?style=shield)](https://circleci.com/gh/TrinityCore/TrinityCore/tree/cata_classic)
[![master Build status](https://ci.appveyor.com/api/projects/status/54d0u1fxe50ad80o/branch/master?svg=true)](https://ci.appveyor.com/project/DDuarte/trinitycore/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/54d0u1fxe50ad80o/branch/3.3.5?svg=true)](https://ci.appveyor.com/project/DDuarte/trinitycore/branch/3.3.5) | [![Build status](https://ci.appveyor.com/api/projects/status/54d0u1fxe50ad80o/branch/cata_classic?svg=true)](https://ci.appveyor.com/project/DDuarte/trinitycore/branch/cata_classic)
[![master Windows Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/win-x64-build.yml/badge.svg?branch=master&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22Windows%20x64%22+branch%3Amaster+event%3Apush) | [![3.3.5 Windows Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/win-x64-build.yml/badge.svg?branch=3.3.5&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22Windows%20x64%22+branch%3A3.3.5+event%3Apush) | [![cata_classic GCC Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/gcc-build.yml/badge.svg?branch=cata_classic&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3AGCC+branch%3Acata_classic+event%3Apush)
[![master Ubuntu Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/linux-build.yml/badge.svg?branch=master&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22Ubuntu%20x64%22+branch%3Amaster+event%3Apush) | [![3.3.5 Ubuntu Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/linux-build.yml/badge.svg?branch=3.3.5&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22Ubuntu%20x64%22+branch%3A3.3.5+event%3Apush) | [![cata_classic GCC Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/gcc-build.yml/badge.svg?branch=cata_classic&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3AGCC+branch%3Acata_classic+event%3Apush)
[![master macOS arm64 Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/macos-arm-build.yml/badge.svg?branch=master&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22macOS%20arm64%22+branch%3Amaster+event%3Apush) | [![3.3.5 macOS arm64 Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/macos-arm-build.yml/badge.svg?branch=3.3.5&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22macOS%20arm64%22+branch%3A3.3.5+event%3Apush) | [![cata_classic macOS arm64 Build status](https://github.com/TrinityCore/TrinityCore/actions/workflows/macos-arm-build.yml/badge.svg?branch=cata_classic&event=push)](https://github.com/TrinityCore/TrinityCore/actions?query=workflow%3A%22macOS%20arm64%22+branch%3Acata_classic+event%3Apush)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/435/badge.svg)](https://scan.coverity.com/projects/435) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/4656/badge.svg)](https://scan.coverity.com/projects/4656) |

## Introduction

TrinityCore is a *MMORPG* Framework based mostly in C++.

It is derived from *MaNGOS*, the *Massive Network Game Object Server*, and is
based on the code of that project with extensive changes over time to optimize,
improve and cleanup the codebase at the same time as improving the in-game
mechanics and functionality.

It is completely open source; community involvement is highly encouraged.

If you wish to contribute ideas or code, please visit our site linked below or
make pull requests to our [Github repository](https://github.com/TrinityCore/TrinityCore/pulls).

For further information on the TrinityCore project, please visit our project
website at [TrinityCore.org](https://www.trinitycore.org).

## Requirements


Software requirements are available in the [wiki](https://trinitycore.info/en/install/requirements) for
Windows, Linux and macOS.


## Install

Detailed installation guides are available in the [wiki](https://trinitycore.info/en/home) for
Windows, Linux and macOS.


## Reporting issues

Issues can be reported via the [Github issue tracker](https://github.com/TrinityCore/TrinityCore/labels/Branch-master).

Please take the time to review existing issues before submitting your own to
prevent duplicates.

In addition, thoroughly read through the [issue tracker guide](https://community.trinitycore.org/topic/37-the-trinitycore-issuetracker-and-you/) to ensure
your report contains the required information. Incorrect or poorly formed
reports are wasteful and are subject to deletion.


## Submitting fixes

C++ fixes are submitted as pull requests via Github. For more information on how to
properly submit a pull request, read the [how-to: maintain a remote fork](https://community.trinitycore.org/topic/9002-howto-maintain-a-remote-fork-for-pull-requests-tortoisegit/).
For SQL only fixes, open a ticket; if a bug report exists for the bug, post on an existing ticket.


## Copyright

License: GPL 2.0

Read file [COPYING](COPYING).


## Authors &amp; Contributors

Read file [AUTHORS](AUTHORS).


## Links

* [Website](https://www.trinitycore.org)
* [Wiki](https://www.trinitycore.info)
* [Forums](https://talk.trinitycore.org/)
* [Discord](https://discord.trinitycore.org/)
