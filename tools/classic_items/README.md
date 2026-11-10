# Vanilla items missing from the 70532 ItemSparse

`gen_hotfix_items.py` writes `hotfixes.item_sparse` rows (plus `hotfix_data`, so clients get them) for vanilla items that the 70532
client has in Item.db2 but not in ItemSparse.db2. The server builds no item template without the ItemSparse row, so these items
do not exist in game: quest rewards come out blank and vendor rows are dropped (list: `checklists/MISSING-ITEMS-QUESTS-VENDORS.md`
in the project files).

Inputs: the 70532 `ItemSparse.db2` and `RandPropPoints.db2`, and a vMaNGOS world database in MariaDB (full dump plus its
`sql/migrations/*_world.sql` up to the core commit of the world baseline, 13b49dc). Needs `pymysql`.

1. `gen_hotfix_items.py --validate ItemSparse.db2 RandPropPoints.db2 <vmangos db>` rebuilds the 12,586 items both sides have and
   prints how many come out the same. On 70532: 92.8% same stats (the rest are Forever's own changes to an item, such as armor or
   spell power added to crafted greens), 90 to 100% same per column.
2. `gen_hotfix_items.py ItemSparse.db2 RandPropPoints.db2 <vmangos db> ids.txt out.sql <first hotfix_data Id> <first UniqueId>`
   writes the rows. Items the client already has are skipped. Equip spells that are not plain stats, use spells and
   chance-on-hit spells are listed at the top of the output.
3. Add `--effects ItemEffect.db2 ItemXItemEffect.db2 SpellEffect.db2` (client files) to write those spells too:
   - an item the client already links spells to (ItemXItemEffect) keeps the client's spells and gets no new ones; a vanilla
     equip bonus whose kind of aura (crit, hit, or the same aura) is among the linked equip spells gives no stat, because the
     client's spell is that bonus now;
   - every other item gets an `item_effect` and `item_x_item_effect` row per spell (same id for both, from 9910001 or
     `--first-effect-id`) with their `hotfix_data` after the item rows. Vanilla cooldowns of 0 are written as -1 (none), as
     the client stores them.
   With the client links the `--validate` check stays at 92.8%.

Stats: value = allocation x RandPropPoints(item level, quality, slot) / 10000, rounded, as `Item::GetItemStatValue` and the
client tooltip compute it. Vanilla percentage bonuses are stored as rating: 14 crit, 10 hit, 12 dodge, 15 parry, 5 block per 1%.
Weapon damage and armor are not in ItemSparse; both sides take them from the item level and quality tables.
