# VMaNGOS creature loot check

Makes `sql/custom/world/2026_10_24_10_world_classic_vmangos_creature_loot.sql` and `reports/vmangos-creature-loot.md`.

1. Load a VMaNGOS world database into MariaDB: the full dump `world_full_14_june_2021.sql` (github.com/brotalnia/database), then
   every `sql/migrations/*_world.sql` of VMaNGOS core after it, up to the core commit the world baseline used (13b49dc). Run
   `vmangos_to_tc.sql` on it (VMaNGOS loot rows in TrinityCore columns, patch 1.12).
2. Rebuild our loot tables: create a database `ours` with `ours_schema.sql`, then load the output of
   `extract_ours.py <repo>/sql ours_loot.sql` (every loot statement of our world SQL in updater order). Add tables `ct` (entry, name)
   and `spawn` (guid, id, map, x, y) from a world dump for the creature list and the starter zones.
3. `items70532.txt`: the ids of the 70532 Item.db2, one per line (`wdc5.py`).
4. `gen_vmangos_loot.py <vmangos db> <out.sql> <out.json> items70532.txt`.

Run it again after any merge that changes an earlier loot file (the sniff import or the baseline): those files are re-applied by
the updater on their own and would bring back the rows this file corrects.
