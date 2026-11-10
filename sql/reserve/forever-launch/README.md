# WoW Forever launch day (4 Nov 2026, 23:00 UK)

Blizzard ships the launch tuning (development notes of 2026-11-03) as a new client build, live hotfixes, or both. This folder
and the steps below take it in quickly. Nothing here runs by itself: no install, update or DB updater applies this folder.

## 1. New client build (only if Battle.net patches the client)

1. Read the build number from the client's `.build.info` (e.g. 1.60.1.7xxxx).
2. Copy what was done for 70707 (commit "Repack: client 70707 ..."): `sql/custom/auth/<date>_auth_classic_<build>.sql`
   (build_info row, realm 70 gamebuild), `repack/client-builds.txt` (new build first, keep the old ones), and the build number in
   repack/NOTICE.txt, build-package.ps1, extract-data.ps1, install-addon.ps1, launcher/Setup.cs, templates/realm.conf.
   The dev launcher and the release launcher get the same change.
3. Extract the DB2 files from the new client (`tools/extract_client_data.ps1`) and run the client diff against 70707
   (`tools/client_diff/client_diff.py`). If a table changed layout (as SpellDuration did in 70707), the server needs a layout update
   in `src/server/game/DataStores/DB2Metadata.h` first, or it won't start. That is the biggest risk.
4. When the new client carries our hotfix rows itself, drop ours. Do the racials first (`hotfixes 2026_10_27_05`): the diff lists
   them as hotfix_other_differ.

## 2. Live hotfixes (every day for the first week)

1. Fetch the live hotfix cache: https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin (public mirror,
   updated daily; wowsims/forever, MIT, builds from it). Its header gives the build. Decoding needs that build's layouts.
2. List what is new since the last push we took. On 2026-11-03 01:10Z that was push 113534, the hunter tuning, already in
   `hotfixes 2026_11_03_40`. The other classes in the notes (druid, mage, paladin, priest, rogue, shaman, warlock, warrior) were
   not in the cache yet.
3. Make the rows with
   `python3 -I tools/full_forever/live_hotfix_sql.py --repo . --dbcache DBCache.bin --spells <ids> --first-id N --last-id M --out x.sql`
   (--dbc is optional now: table hashes come from the names). Use a new 100-id block of hotfix_data ids; 2001600-2001699 is the
   hunter file.
4. Compare the class spells again (same method as `world 2026_11_03_40`). Drop the vanilla value row wherever Forever is now within
   30% of vanilla or stronger, and keep it where Forever is still 30%+ lower (Karrz rule; `full-forever/04` holds those rows).

## 3. Trainers and removed spells (after step 1 or 2 has removed them)

- `01_world_trainers_launch.sql` (world): trainers stop teaching Frost Blast, Lava Burst, Blessing of Salvation / Light (and the
  Greater ones). Righteous Fury trains at level 10. (Lacerate stays: its ranks train after the talent, world 2026_11_03_42.) Undo: `01_undo.sql`.
- `02_characters_unlearn_removed.sql` (characters): characters forget the removed spells. Undo: `02_undo.sql`.
- New ranks already live (Ferocious Bite rank 1 at 24, Lacerate rank 2 at 34) are in world 2026_11_03_42. The others (new Rogue
  ability at 24, Death Mark, Reckless Swing ...) need their spell ids from the new data before trainer rows can be made.
- Training costs: the notes say 206-207 gold in total for 1-60 per class. Ours (from Forever sniffs) already total 201-212 gold for
  warrior, hunter, rogue, shaman and druid; warlock 134 (Grimoires come on top). Paladin, priest and mage come out at 260-270 only
  because their trainers list both factions' and all races' spells. No change needed.
- Bots (PlayerbotV2): every rotation and blessing check asks knows_spell first, so a removed spell is simply not cast (Lava Burst,
  Frost Blast, Salvation, Light). The paladin blessing plan leaves unknown blessings out before it splits them between paladins, so
  two paladins never both land on the same one. The talent builds name the launch talents (ClassicTalentBuilds.cpp: Precision,
  Accelerant, Missile Barrage, Oath of Light, Shieldsworn, Unsealed Wrath, the druid node swaps). After a new live push, check the log
  for "build talents not found".

## 4. Mob health

Our creatures use vanilla health (`creature_classic_stats`, Creature::SelectLevel); the client's ExpectedStat is only used for
Forever-only creatures. So Forever's "more health on higher level creatures" does NOT reach our mobs by itself. To follow it:
compare the old and new ExpectedStat CreatureHealth (and CreatureDifficulty HealthModifier) per level, and scale
`creature_classic_stats` health by the same ratio per level. Then run the dungeon scenarios. Needs the new data first.

## 5. Test before the PC

Each SQL file twice on a scratch DB, then on the test install (never the live realm): start worldserver, check DBErrors.log, run
the bot DPS and dungeon scenarios. Nothing goes on Karrz's PC without his go.
