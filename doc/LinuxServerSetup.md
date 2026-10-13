# Forever server on Linux (Ubuntu 24.04) - setup log

Status: **first full run works (2026-10-13)**, guide still being cleaned up. This is the record of setting up a clean public server on a fresh
DigitalOcean droplet and connecting to it from a separate Windows PC. Every command here was run
for real; problems found along the way are listed under [Findings](#findings). It will become the
Linux guide once the first full run works, and a Windows guide follows from it.

Test machine: DigitalOcean `s-2vcpu-4gb-amd` (2 vCPU, 4 GB RAM, 80 GB disk), Ubuntu 24.04.5 LTS,
branch `forever` at `669c8de4ec`.

## 1. SSH access

A password login works for a person, but a key is needed for scripted work. On the Windows PC
(Git Bash or PowerShell, OpenSSH is built into Windows 10+):

```
ssh-keygen -t ed25519 -C "forever-droplet" -f ~/.ssh/forever_droplet
```

Install the public key on the server (asks for the root password once):

```powershell
Get-Content $env:USERPROFILE\.ssh\forever_droplet.pub | ssh root@SERVER_IP "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys"
```

Then: `ssh -i ~/.ssh/forever_droplet root@SERVER_IP`.

## 2. Work inside `screen`

Long jobs (the build takes hours) must survive a dropped SSH connection. All commands below were
run in one shared, logged screen session:

```
screen -dmS forever -L -Logfile /root/forever-screen.log
screen -x forever            # attach (several people can watch at once)
```

Detach with `Ctrl+A` then `D`. Do **not** press `Ctrl+C` or type `exit` inside it - that stops the
running job. Scroll back: `Ctrl+A`, `Esc`, arrows/PgUp, `Esc` to return.

## 3. Swap (needed on 4 GB machines)

GCC needs more than 4 GB on the largest files with `-j2`. Without swap the build gets killed.

```
fallocate -l 8G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
echo "/swapfile none swap sw 0 0" >> /etc/fstab
```

## 4. Packages

```
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y git clang cmake make gcc g++ libmysqlclient-dev libssl-dev libbz2-dev \
  libreadline-dev libncurses-dev libboost-all-dev mysql-server p7zip-full
```

Versions installed: CMake 3.28.3 (needs 3.24+), GCC 13.3.0, Boost 1.83 (needs 1.78+),
MySQL 8.0.46.

## 5. Source

```
cd /root
git clone --branch forever --single-branch --depth 1 https://github.com/advocaite/TrinityCore.git TrinityCore
```

(~560 MB download, 4.9 GB on disk.)

## 6. Configure and build

```
mkdir -p /root/TrinityCore/build && cd /root/TrinityCore/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/root/server -DTOOLS=0 -DWITH_WARNINGS=0
make -k -j2 2>&1 | tee /root/build.log
```

- `Release`: debug info makes linking much heavier on a 4 GB machine.
- `TOOLS=0`: the map extractors run on Windows next to the client, not on the server.
- `-j2` on 2 vCPU / 4 GB + swap. `-k` keeps going after a compile error so one run shows all errors.
- Build time on this droplet: **48 minutes**, no compile errors with GCC 13.3.

Then `make install` (puts `bin/`, `etc/*.conf.dist` and `lib/` under `/root/server`).

## 7. Game data

Extracted on the Windows PC with the client (see README "Game data"), then copied to the server.
Needed: `dbc`, `gt`, `maps`, `vmaps`, `cameras`. **No `mmaps`** (1.6 GB, not needed; `mmap.enablePathFinding = 0`).

```
tar -cf - -C "PATH/TO/DATADIR" dbc gt maps vmaps cameras | ssh root@SERVER_IP "mkdir -p /root/server/data && tar -xf - -C /root/server/data"
```

(~680 MB.)

## 8. Databases

world + hotfixes come from a dump of the dev database (the plan is to ship this dump instead of
TDB). auth + characters are created fresh by worldserver's auto-setup, so no accounts or
characters leave the dev machine.

Dump on the dev PC (MySQL 8.4 client):

```
mysqldump --host=127.0.0.1 --port=3306 --user=trinity -p --single-transaction --hex-blob \
  --set-gtid-purged=OFF --no-tablespaces --databases world hotfixes \
  --ignore-table=world.updates_apply_check --ignore-table=hotfixes.updates_apply_check \
  --result-file=forever_db.sql
gzip -4 forever_db.sql          # 985 MB -> 277 MB
scp forever_db.sql.gz root@SERVER_IP:/root/
```

MySQL user (password generated and kept in a root-only file; MySQL on Ubuntu already listens on
127.0.0.1 only, which is what we want):

```
P=$(openssl rand -hex 16); umask 077; echo "$P" > /root/.forever_db_pass
mysql -e "CREATE USER trinity@localhost IDENTIFIED BY '$P';
  GRANT ALL PRIVILEGES ON world.* TO trinity@localhost;  GRANT ALL PRIVILEGES ON hotfixes.* TO trinity@localhost;
  GRANT ALL PRIVILEGES ON auth.* TO trinity@localhost;   GRANT ALL PRIVILEGES ON characters.* TO trinity@localhost;"
```

Import (4 minutes on the droplet):

```
zcat /root/forever_db.sql.gz | mysql
```

Check: same row counts as the dev DB (world.creature 79422, hotfixes.hotfix_data 24304).

## 9. Configs, certificate, firewall

Configs from the `.dist` files with only the needed keys changed:

```
sh contrib/linux/configure_server.sh /root/server SERVER_IP "$(cat /root/.forever_db_pass)"
```

Certificate: the server certificate chain + key for `trinity.actual.battle.net` (NOT the root CA
key, that never leaves the PC that made it):

```
scp trinity_forever.cert.pem root@SERVER_IP:/root/server/bin/forever.cert.pem
scp trinity_forever.key.pem  root@SERVER_IP:/root/server/bin/forever.key.pem
chmod 600 /root/server/bin/forever.key.pem
```

The server must resolve the login hostname itself (Finding 2):

```
echo "SERVER_IP trinity.actual.battle.net" >> /etc/hosts
```

Firewall (SSH first, so you do not lock yourself out):

```
ufw allow OpenSSH
ufw allow 1119/tcp      # bnetserver
ufw allow 8081/tcp      # login REST (LoginREST.Port)
ufw allow 8085/tcp      # worldserver
ufw --force enable
```

MySQL (3306), Remote Access (3443) and SOAP (7878) stay closed.

## 10. First start

worldserver, in its own screen window, from the bin folder:

```
cd /root/server/bin && ./worldserver -c /root/server/etc/worldserver.conf
```

Answer `yes` to create `auth` and `characters` (Finding 7). It then applies all updates.
"World initialized in 2 minutes 38 seconds" on the droplet.

Point the realm at the public IP (Finding 4; `localAddress` stays 127.0.0.1):

```
mysql -e "UPDATE auth.realmlist SET address='SERVER_IP' WHERE id=70"
```

bnetserver, in another screen window, **from the bin folder** (Finding 10):

```
cd /root/server/bin && ./bnetserver -c /root/server/etc/bnetserver.conf
```

Log shows `Added realm "Trinity" at SERVER_IP 127.0.0.1 (port 8085)`. `ss -ltnp` shows 1119, 8081
(bnetserver) and 8085 (worldserver) on `0.0.0.0`.

Check from the client PC before starting the game (both must say `Verify return code: 0 (ok)`):

```
openssl s_client -connect SERVER_IP:8081 -servername trinity.actual.battle.net -CAfile root_ca.cert.pem
openssl s_client -connect SERVER_IP:1119 -servername trinity.actual.battle.net -CAfile root_ca.cert.pem
```

PowerShell: `Test-NetConnection SERVER_IP -Port 8085` must say `TcpTestSucceeded : True`.

## 11. Connecting from a client PC

Account (worldserver console, i.e. the screen window running worldserver):

```
bnetaccount create EMAIL PASSWORD
account set gmlevel 1#1 3 -1        # optional, GM for the first game account
```

On the client PC, the only change is the hosts entry (edit as Administrator; keep a copy first):

```
SERVER_IP trinity.actual.battle.net
```

then `ipconfig /flushdns`, and start the game with the Forever Launcher as usual. The PC must
trust the root CA that signed the server certificate (same as for a local server).

**Result 2026-10-13: works.** Logged in from a home PC, created a character, entered the world
(server saw the home IP, not 127.0.0.1).

Memory on the 4 GB droplet with one player in the world: 3.6 GB RAM used plus 1.7 GB swap. One
realm fits but is tight; more realms or players need 8 GB.

## Findings

Problems that explain why connecting to an outside server fails, and anything that differed from
the Windows dev setup.

1. **The repack is local-only.** Its `bnetserver.conf` and `worldserver.conf` both set
   `BindIP = "127.0.0.1"`, so the servers do not accept connections from any other machine.
   Hosting the repack for others needs `BindIP = "0.0.0.0"` in both (plus items 2-4).
2. **The server itself needs the hosts entry.** `LoginREST.ExternalAddress` and
   `LoginREST.LocalAddress` are not bind addresses (that is `BindIP`); they are the hostnames sent
   back to clients. But bnetserver resolves both at startup and **refuses to start** if either
   does not resolve ("Could not resolve LoginREST.ExternalAddress", LoginRESTService.cpp).
   `trinity.actual.battle.net` does not exist in public DNS (verified on the droplet), so the
   server needs `SERVER_PUBLIC_IP trinity.actual.battle.net` in its own `/etc/hosts`
   (Windows: `C:\Windows\System32\drivers\etc\hosts`).
3. **`LoginREST.ExternalAddress` must stay the hostname**, not the IP: the client opens TLS to that
   name and the certificate only covers `trinity.actual.battle.net` / `*.actual.battle.net`.
4. **realmlist address**: `auth.realmlist.address` must be the public IP, or clients are sent to
   127.0.0.1 after login. To verify.
5. **`Network.EnterEncryptedModeRegionGroup` is not in `worldserver.conf.dist`.** It defaults to 0;
   the client needs 8. Only the README mentions it. `contrib/linux/configure_server.sh` adds it.
6. **SQL file hashes differed between Windows and Linux (fixed in the updater).** The world
   baselines (`2026_09_27_00_world_forever_baseline_01/02.sql`) contain CRLF bytes inside text
   strings in their git copy. `UpdateFetcher::ReadSQLUpdate` read files in text mode, which turns
   CRLF into LF only on Windows, so Windows stored hash `2AB2CF9` and Linux computed `6FDF0A5`. A
   database made on Windows (dev, repack, our dump) therefore made a Linux worldserver re-run the
   65 MB baseline: it failed with `error 1136 ... Column count doesn't match value count` after
   already changing rows (creature 79422 -> 77149), and if it had succeeded it would have re-run
   every later custom file too. Fix: ReadSQLUpdate reads binary and converts CRLF to LF itself, which
   is the hash Windows has always stored, so no existing database sees a change.
   After a failed run like this, re-import world from the dump before starting again.
7. **First start asks questions.** With `Updates.AutoSetup = 1`, worldserver asks
   `Database "auth" does not exist, do you want to create it?` (and the same for `characters`).
   Run the first start in the foreground (screen) and answer `yes`.
8. **worldserver needs the vmaps to start.** Without `vmaps/GameObjectModels.dtree` it stops with
   "Unable to load gameobject models ... server shutting down". Upload all game data before the
   first start (the database setup part of that first start still completed). Also
   `Classic.Hardcore` is missing from `worldserver.conf.dist` (only a warning, default 0 is right
   for a Normal realm).
9. **About a million DB warnings on every start.** With the `.dist` logger setting
   (`Logger.sql.sql = 5,Console DBErrors`) they all scroll through the console and make it look
   broken. They are the same on the dev server (retail rows pointing at creature displays,
   factions, items etc. that the Classic client data does not have; the server skips them).
   `configure_server.sh` sets `Logger.sql.sql = 5,DBErrors` (file only), like the dev and repack
   configs.
10. **Every repack has its own certificate authority.** The repack's Setup.bat creates a root CA
   for that one PC (and deletes its key), so a friend's PC does not trust the host's login
   certificate unless the host's `server\forever_root_ca.crt` is imported there. This test only
   worked without that step because the client PC already trusted the CA that signed the server
   certificate. The repack's `setup\host-for-others.bat` now builds a friend kit that does it.
11. **Certificate paths are relative to the working directory.** `CertificatesFile = "./forever.cert.pem"`
   means bnetserver must be started from `/root/server/bin`.
