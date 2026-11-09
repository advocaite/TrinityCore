# Forever cache tools

Read the cache files the WoW Forever client writes on players' computers and compare them with our world database.
Players send two folders from their game folder: `_forever_\Cache\WDB` and `_forever_\Cache\ADB`. Never the WTF folder.

1. `wdb.py` reads `Cache\WDB\<locale>\questcache.wdb`, `creaturecache.wdb` and `gameobjectcache.wdb` into one JSON file:
   `python3 -I tools/forever_cache/wdb.py <player>/Cache/WDB --out <player>.json`
2. `compare.py` compares one or more of those JSON files with the world database (read only) and writes a report and SQL drafts:
   `python3 -I tools/forever_cache/compare.py a.json b.json ... --db world --outdir out [--min-caches 2]`
   - `quests.tsv`: each cached quest is missing, placeholder, text differs or same.
   - `placeholder_quests.sql`: real text, objectives, levels and rewards for our placeholder quests (VerifiedBuild -1).
   - `quest_locales.sql`: Russian (and other) quest texts where ours are missing.
   - `gameobjects_missing.sql`: object templates we lack (no spawns). `creatures.tsv`: NPCs we lack or that look different
     (report only: faction, level and NPC flags are not in the cache).
   - `missing_quests.json`: full data of quests we lack, for the quest builder.
3. Items: `Cache\ADB\<locale>\DBCache.bin` has the same format as the live hotfix file, so use
   `tools/full_forever/live_hotfix_sql.py --dbcache <player>/Cache/ADB/enUS/DBCache.bin` with its own hotfix id range.
4. Items only a Russian (or other non-English) cache has: `english_names.py` writes the same records with the real English name
   (Questie's classic item data, then wowsims/forever's item data) and the Russian text as the locale row. Where no source has
   the English name yet, it uses our own translation from `item_translations.tsv`; swap those once an English cache has them.

A cache from someone who also plays on our realm holds our own answers too. That is why only quests that are missing
or placeholders on our side become SQL. Differences on quests we already have are listed, never changed automatically.
The cache has no quest reward or progress text (those come with the NPC's answer), so placeholder reward text stays.
