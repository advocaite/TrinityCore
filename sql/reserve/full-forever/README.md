# Full Forever reserve

This folder holds the switch from "vanilla where Forever is weaker" to "as close to live WoW Forever as we can". Nothing here is
applied by an install, an update or the DB updater; it only runs by hand. Karrz (2026-10-30): keep the vanilla values for now,
hold everything in reserve, and swap it all over once Forever's gear, balance and data are there.

Live data comes from Blizzard's live hotfix cache, the one wowsims/forever (MIT, https://github.com/wowsims/forever) builds its
data from (public mirror: https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin).

| File | DB | What it does | Undo |
| --- | --- | --- | --- |
| 01_hotfixes_live_spell_values.sql | hotfixes | live values for spells we hold vanilla: Aspect of the Hawk, Trueshot Aura, Lesser Intellect | 01_undo.sql |
| 02_world_drop_vanilla_holds.sql | world | drops the vanilla value rows on those spells (and Accelerant's -0.1 sec) | 02_undo.sql |
| 03_hotfixes_live_invalid_items.sql | hotfixes | drops the 12 items live marks invalid (Aegis of Stormwind ...) | 03_undo.sql |
| 04_world_drop_all_class_vanilla_holds.sql | world | the big one: every vanilla value / cost / timing row on class spells (2,264 + 1,562 + 41 rows) | 04_undo.sql |

Since 2026-11-03 01 is out of date: Forever's launch tuning raised Aspect of the Hawk and Trueshot Aura back to vanilla
(hotfixes 2026_11_03_40), so do not run 01 or 02. 01-03 are the holds found in the 2026-10-30 check against live (push 113511). 04 covers every class spell, including the ones
live hasn't changed yet, so after 04 every player and bot class spell uses the client's numbers plus our live hotfixes.

## How to switch

1. Refresh the live data first, because live will have moved on by then. Take the cache for the build the server reads (the
   DBCache.bin above, or the PC client's `_forever_\Cache\ADB\enUS\DBCache.bin`) and that build's `server\data\dbc\enUS`.
   Then regenerate 01 with the spells you want:

       python3 -I tools/full_forever/live_hotfix_sql.py --repo . --dbcache DBCache.bin --dbc <dbc\enUS> \
           --spells 13165,14318,14319,14320,14321,19506,20905,1299346,1299348,3166 --first-id 2001500 --last-id 2001549 --out x.sql

   Rerun the comparison as well (see the "wowsims crossref" report), so newly changed spells get added.
2. Test on a copy (the C:\advo-test MySQL or a test install), never the live realm: apply 01, 02, 03, then 04, each one twice.
   Start the worldserver and check DBErrors.log.
3. Run the bot DPS / heal scenarios. The baselines will drop, because 70685 bases are 30-57% below vanilla on many ranks.
4. To go back: run the undo files in reverse order (04, 03, 02, 01).

Hotfix ids reserved for this folder: 2001500-2001599.
