# Installing the ForeverBots bots

You get: party companions you add with one click, raid/dungeon/battleground bots, bots roaming the
world (about 300), an auction house that is stocked and trades, and the in-game `ForeverBots` window.
This is advocaite's `forever` tree with the bots in it. Feedback and logs are welcome (see the end).

Two ways to get a server with the bots. Both end with the same steps 3-6.

## A. You already run the Forever repack (Windows)

The bots are part of the server program, so you need a `worldserver.exe` that was built with them.

1. **Get that worldserver.exe.** Either use a prebuilt one if the person who gave you this
   package included it, or build it yourself (section B, steps 1-2). Close the server, then
   **back up your `characters` database** (the bots add a few tables to it) and keep a copy of
   your old `worldserver.exe`.
2. Copy the new `worldserver.exe` over the old one. If a `worldserver.conf.d` folder does not
   exist next to it, create it.

## B. Building from source

1. Build as for the plain Forever server, adding one option:
   `cmake -S . -B build -DSERVERS=1 -DSCRIPTS=static -DBUILD_PLAYERBOT_V2=1`
   then `cmake --build build --config RelWithDebInfo --target worldserver`. Needs the usual
   TrinityCore requirements (Visual Studio 2022 or newer on Windows, Boost, OpenSSL, MySQL
   8 client). Without `-DBUILD_PLAYERBOT_V2=1` you get the unchanged server and no bots.
2. The build puts `playerbot.conf.dist` into `worldserver.conf.d`. Run `cmake --install` or copy
   the files from `build\...\worldserver.conf.d` next to your `worldserver.exe`.
   Keep `SourceDirectory` in `worldserver.conf` pointing at this checkout: on first start the
   server applies the SQL files in `sql\playerbot_v2` from there.

## Steps for everyone

Optional first: the vanilla (1.12) numbers. They are not in the repository; they are built from the VMaNGOS database dump you download yourself,
with a builder script that is not part of this branch. Without them the bots and the server use the client's own values.

3. **Create the shared bot database (once).** In a command prompt, from this folder
   (use your own MySQL user; `mysql.exe` is in your MySQL `bin` folder):
   ```
   mysql -u root -p < sql\playerbot_shared\playerbot_shared_schema.sql
   mysql -u root -p playerbot < sql\playerbot_shared\playerbot_shared_data.sql
   ```
   Your database user needs rights on the new `playerbot` database
   (`GRANT ALL ON playerbot.* TO '<your db user>'@'%';`). The server reads and writes it with the
   same login as your other databases. The files do not drop anything.
4. **Settings.** The build puts `zz-forever-bots.conf` into `worldserver.conf.d` (an existing copy is never overwritten).
   It turns on the world bots (up to 220 awake, about 160 around each player, from a roster of 3000), the auction house bot, bot raid tactics and bot logging, so they work
   without further setup. The raid and instance shortcuts that are not Blizzlike (solo raid entry, no attunements, no Molten Core runes,
   open Ahn'Qiraj gates; see `doc/OptionalServerOptions.md`) are in that file but commented out; remove the `#` to use them.
   Optionally rename `playerbot.conf.dist` to `playerbot.conf` to change other bot settings (every key is documented there).
   `examples\zz-forever-bots.conf` is the older optional version (shortcuts switched on, faster raid casting, 300 bots); copy it over to use that.
   Change a value and restart to tune; set `Playerbot.World.Enable = 0` for no roaming bots,
   `AuctionHouseBot.Seller.Enabled = 0` for no auction bot. Weak PC: set `Playerbot.World.MaxBots = 100`,
   `BotsPerPlayer = 40`, `CityBots = 40` and `ZoneBots = 20`. Expect about 2.9 GB of RAM for the server plus about 4 MB per awake bot (about 3.8 GB at 220 bots).
5. **Start the server** (bnetserver, then worldserver). The first start takes longer while the bot
   tables are created. Bot log lines go to `Playerbot.log` in your logs folder.
6. **Install the addon.** Copy the folder `src\modules\PlayerbotV2\Addon\ForeverBots` into
   `<your WoW folder>\_forever_\Interface\AddOns\` (so you have `...\AddOns\ForeverBots\ForeverBots.toc`).
   On the character screen open AddOns and tick "Load out of date AddOns" if it is greyed out.

## Using the bots in game

Your account must be a GM account for the `.bot` commands and the window (in the worldserver
console: `account set gmlevel <accountname> 3 -1`).

- `/fb` opens the window: one button per class and role adds a bot to your group at your level,
  fully geared. Buttons for Follow, Stay, Attack, Pull, Flee, Release (after a wipe), formations, raid
  icons and tank assignments. Everything also works by typing the word in party/raid chat.
- Full command list (chat orders, `.bot`, addon, marks, raids): `src/modules/PlayerbotV2/BOT_COMMANDS.md`.
- Commands: `.bot add <class> <role>`, `.bot call all`, `.bot dismiss`, `.bot regear`, `.bot save <name|all>`,
  `.bot delete <name>`, `.bot world` (shows the roaming population). `.bot` lists the rest.
- Raids: add the bots, enter the instance, use the window's Pull and Release. A good Molten Core
  group is 4 tanks, 12 healers, 6 mages, 4 warlocks, 3 hunters, 5 rogues, 4 warriors.
- Battlegrounds: queue yourself; bots fill the other places.

## Updating, removing, reporting problems

- Update: replace `worldserver.exe`, the `examples` file and the addon; new SQL is
  applied by the server at start. Back up `characters` first.
- Remove: delete `playerbot.conf` and `zz-forever-bots.conf` (if you used it), and use a worldserver built without the
  option. The extra tables are harmless.
- Report a problem with: what you did, `Playerbot.log`, `Server.log`, and the time it happened. Useful
  lines are tagged `[raid_*]`, `[bot_death]`, `[cast_fail]`, `[world]`. Harmless messages:
  "Invalid BattlegroundMapScript", "OpenSSL failed to load legacy.dll".
- Needs 40-player raid content from the Forever core; the bots do not change which content exists.

Credits and licence: `BOTS.md`, `src/modules/PlayerbotV2/NOTICE.md` (GPL-2.0).
