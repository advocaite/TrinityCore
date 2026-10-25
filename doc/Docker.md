# Running the server in Docker

[`contrib/docker-compose`](../contrib/docker-compose) runs this branch in Docker: MySQL, worldserver and bnetserver,
plus three one-shot helpers. `tdb` downloads the Forever database release, `realm` sets the realm's address, and
`extractor` extracts the game data from a client. MySQL is not published. The game uses ports 1119, 8081 and 8085, and
the console's SOAP port 7878 is published on the host itself only.

You need:

- Docker with Compose v2: Docker Desktop on macOS and Windows, or Docker Engine on Linux.
- Memory: about 8 GB to build (each compile job needs 2.5 GB; with less memory the build runs fewer jobs), and about
  5 GB to run.
- The WoW Classic beta client 1.60.1.70450, for the game data and to play.

All commands below run in `contrib/docker-compose`.

## First start

1. **Settings.** `cp .env.example .env`, fill in the two passwords (`openssl rand -hex 16`), and set `WOW_DIR` to the
   client's World of Warcraft folder. `.env` is git-ignored.
2. **Certificates.** `./make-certs.sh` creates your own certificate authority and the bnetserver certificate in
   `certs/`, which is git-ignored. Players import `certs/ca.cert.pem`; `certs/ca.key.pem` never leaves this machine.
3. **Build:** `docker compose build worldserver`. The first build takes about an hour on four cores. After a
   `git pull`, only what changed is compiled again.
4. **Game data** from the client into `DATA_DIR` (about 700 MB): `docker compose run --rm extractor`
5. **Start:** `docker compose up -d`. The first start downloads and imports the databases (about 1 GB), which takes a
   few minutes; `up` waits for it. Follow it with `docker compose logs -f worldserver`.
6. **Account.** `docker compose attach worldserver`, then:
   ```
   bnetaccount create you@example.com yourpassword
   account set gmlevel 1#1 3 -1
   ```
   Detach with **Ctrl+P, Ctrl+Q**: Ctrl+C would shut the server down.
7. **Check the login TLS.** Both commands must end with `Verify return code: 0 (ok)`:
   ```
   openssl s_client -connect 127.0.0.1:1119 -servername trinity.actual.battle.net -CAfile certs/ca.cert.pem </dev/null
   openssl s_client -connect 127.0.0.1:8081 -servername trinity.actual.battle.net -CAfile certs/ca.cert.pem </dev/null
   ```

## Playing

Each player needs:

- the 1.60.1.70450 client;
- `certs/ca.cert.pem` imported as a trusted root certificate;
- a hosts entry `PUBLIC_IP trinity.actual.battle.net`.

They then start the client with the Forever Launcher, as described in [Building from source](../README.md#building-from-source).
In the realm list, choose **Trinity**: the other Forever realms have no worldserver in this setup.

For players on other machines:

1. Set `PUBLIC_IP` to the address they reach this machine at.
2. Set `PUBLISH_IP` to the address the ports are published on. `0.0.0.0` means every network; then limit who can connect
   with a firewall.
3. Run `docker compose up -d` again.

## Day to day

- **Update:** `git pull`, `docker compose build worldserver`, `docker compose up -d`. The worldserver applies new SQL
  updates when it starts.
- **Logs:** `docker compose logs -f worldserver`. The server's log files are in the `logs` volume.
- **Backup** of accounts and characters:
  `docker compose exec db sh -c 'mysqldump -uroot -p"$MYSQL_ROOT_PASSWORD" --databases auth characters' > backup.sql`
- `docker compose down` stops everything. With `-v` it also **deletes the databases**.

## Spell test module

The spell test module, [`src/modules/SpellTest`](../src/modules/SpellTest), tests spells without a game client. Its
`.spelltest` GM commands make test characters, which cast, attack and take hits when told to, and log everything they
cast, deal, gain and proc. Use it to check a spell against its tooltip, or to check a fix before a pull request.

### Setting it up

1. **Build it in:** set `BUILD_SPELL_TEST=1` in `.env`, then run `docker compose build worldserver` and
   `docker compose up -d`.
2. **SOAP account.** In the console (`docker compose attach worldserver`), make an account for the SOAP console with
   GM level 3. Game account passwords have at most 16 characters.
   ```
   account create spelltester yourpassword
   account set gmlevel spelltester 3 -1
   ```
3. **`soap.env`.** Write it next to `spelltest.sh`; it is git-ignored:
   ```
   SOAP_USER=spelltester
   SOAP_PASS=yourpassword
   ```
4. **Check:** `./spelltest.sh run "server info"`

`spelltest.sh run COMMAND...` runs console commands. `spelltest.sh script FILE` runs a file of them, one per line, where
`sleep SECONDS` waits and `#` starts a comment. [`spelltest/example.txt`](../contrib/docker-compose/spelltest/example.txt)
is an example. The commands also work in the console and as GM chat commands.

### Commands

Test characters are characters of the account `SPELLTEST`, which `.spelltest create` makes on first use. They start at
level 60 on GM Island, away from players, with their weapon skills at the maximum. Every login gives them full health
and power.

| Command | What it does |
|---|---|
| `create NAME RACE CLASS [GENDER] [LEVEL]` | Makes a new test character. Race and class are numbers or vanilla names (`undead`, `mage`). |
| `login NAME`, `logout NAME`, `status` | Logs a test character in or out, or lists the logged-in ones. |
| `info SPELL` | Shows the spell as the server loaded it: attributes, effects, proc data, scripts. |
| `dummy NAME [YARDS] [ENTRY]` | Puts a fresh target dummy in front of the character. Default: Unkillable Test Dummy (9659), level 60. |
| `place NAME OTHER [YARDS]` | Moves the test character OTHER in front of NAME. |
| `pvp NAME on\|off` | Turns free-for-all PvP on or off. Two test characters that both have it can attack each other. |
| `cast NAME SPELL [TARGET]` | A normal cast: cast time, cost and cooldown apply. |
| `trigger NAME SPELL [TARGET]` | A triggered cast: no cast time, cost or cooldown. |
| `attack NAME [TARGET]`, `stop NAME` | Starts melee; stops attacking and casting. |
| `learn NAME SPELL`, `unlearn NAME SPELL`, `equip NAME ITEM` | Teaches or removes a spell; gives and equips an item. |
| `reset NAME` | Brings the character back to life, resets cooldowns, health and power, and removes the auras that are not passive. |
| `health NAME PERCENT [TARGET]`, `auras NAME [TARGET]`, `stats NAME` | Sets health; lists auras; shows the character's stats. |
| `log NAME` | Prints the character's log, then clears it. |

TARGET is `dummy`, `self`, `none`, or another test character's name. Without a TARGET:

- `cast` and `trigger` take the dummy if there is one, and otherwise the character itself;
- `attack` takes the dummy;
- `health` and `auras` take the character itself.

The dummy never dies: a killing blow restores its health. It doesn't move or attack, and it despawns after two hours.
Other vanilla test dummies can be given as ENTRY, such as 12317 (low armor), or 12916 and 12917 (low and high magic
resistances).

### The log

The log holds what the character, its dummy and the units it owns (pets, totems) cast, deal, gain and proc. Each line
starts with the game time in seconds since the last `log`. Right after a login it also holds what the login cast
(passive skills, racials), so run `log` once before a test to clear it.

A Fireball (rank 1) on the dummy:

```
   1.523 cast 133 Fireball Testmage -> Unkillable Test Dummy
   2.298 dmg 133 Fireball Testmage -> Unkillable Test Dummy: 19 (before mitigation 19, absorbed 0, resisted 0, blocked 0, school 0x4)
   2.298 aura+ 133 Fireball on Unkillable Test Dummy from Testmage: 4000/4000 ms, stacks 1, charges 0; e1 aura 3 amount 1 every 2000 ms
```

An aura that procs, here Touch of the Grave on a melee swing:

```
  11.497 proc 1260189 Touch of the Grave on Gravewar from a swing (Gravewar -> Unkillable Test Dummy): type 0x400004:0x0, spell type 0x0, phase 0x0, hit 0x1, effects 0x1
```

The other lines are `miss`, `fail` (a cast that failed, with the reason), `tick` (periodic damage or healing), `heal`,
`energize`, `swing` (melee), `damage shield`, `absorb`, `execute`, and `aura-` (an aura removed, with how).
