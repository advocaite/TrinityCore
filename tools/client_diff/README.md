# Client build diff

Reruns the "what did the new client change, and what does our server miss or hold differently" check whenever a client
build lands or a hotfix/world import changes our data.

## Inputs

- `--old`, `--new`: folders of `.db2` files. On the PC that is `server\data\dbc\enUS` after `mapextractor` (copy the old
  folder away before extracting the new build). The old folder may hold only some tables; the rest are listed as
  "not compared".
- `--hotfixes`, `--world`: mysqldump files of our databases (`backups\<commit>\hotfixes.sql.gz` and `world.sql.gz`).
- `--pending-sql` (optional): SQL files or patches queued for the next build, so the classic_spell_info durations they
  already clear are not reported again.

Field names and layouts come from `src/server/game/DataStores/DB2Metadata.h` and `DB2LoadInfo.h`, so a table reads
the way the worldserver reads it. A layout hash that does not match the source shows up in the report's table list.

## Run

    python3 -I tools/client_diff/client_diff.py --repo . \
        --old dbc-70450 --new dbc-70532 --old-label 70450 --new-label 70532 \
        --hotfixes hotfixes.sql.gz --world world.sql.gz \
        --pending-sql path/to/queued/sql --out client-diff-70532

Writes `report.html` (one page, grouped into missing / different / new and unused / changed in the client),
`report.json` and one TSV per list. A full run takes about a minute.

## What it checks

- Client build to build: ItemSparse (item level, required level, stats as the server computes them), item sets, spell
  rows (name, misc, effects, aura options, categories, class options, and cooldowns/levels/power when both builds have
  them), talent definitions, encrypted (hidden) record counts.
- Hotfix rows against the new client record: same, different, hotfix-only, and rows not listed in `hotfix_data`.
- `classic_spell_info` (cast, cooldown, duration, range) and `classic_spell_cost` against the client, and
  `classic_spell_value` rows sitting on effects the new build changed.
- Loot, vendor and quest reward rows that point at items the server cannot build (no ItemSparse record, no hotfix).
- Client quests with no spawned giver, trainer spells the game lacks or offers at another level, class spells the
  client marks as trainer-learned that nothing teaches, spawned NPCs with no client model, new spells and items nothing
  hands out.
