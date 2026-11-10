#!/usr/bin/env python3
# Compare our creature loot (rebuilt in db `ours`) with vMaNGOS 1.12 (converted in db VM as tc_*) and write the fix SQL + report data.
import json, sys, collections, struct

def f32(x):
    return float('%.6g' % struct.unpack('<f', struct.pack('<f', float(x)))[0])
import pymysql

# usage: gen_vmangos_loot.py <vmangos db> <out.sql> <out.json> <items70532.txt> [mysql socket]
# db `ours` = our loot tables rebuilt by extract_ours.py; the VMaNGOS db needs vmangos_to_tc.sql run on it first
VM = sys.argv[1]
OUT_SQL = sys.argv[2]
OUT_JSON = sys.argv[3]
ITEMS = sys.argv[4]
SOCK = sys.argv[5] if len(sys.argv) > 5 else '/run/mysqld/mysqld.sock'

db = pymysql.connect(unix_socket=SOCK, user='root')
cur = db.cursor()

def q(sql, *a):
    cur.execute(sql, a)
    return cur.fetchall()

items70532 = set(int(x) for x in open(ITEMS))
vm_items = set(r[0] for r in q(f"SELECT DISTINCT entry FROM {VM}.item_template"))
vm_item_name = dict(q(f"SELECT entry, name FROM {VM}.item_template t WHERE patch = (SELECT MAX(patch) FROM {VM}.item_template t2 WHERE t2.entry = t.entry AND t2.patch <= 10)"))

# creatures
vm_ct = {r[0]: r for r in q(f"SELECT entry, name, loot_id, skinning_loot_id, pickpocket_loot_id, gold_min, gold_max FROM {VM}.tc_ct")}
our_ct = dict(q("SELECT entry, name FROM ours.ct"))
our_diff = {r[0]: r for r in q("SELECT Entry, LootID, SkinLootID, PickPocketLootID, GoldMin, GoldMax FROM ours.creature_template_difficulty WHERE DifficultyID = 0")}
spawns = collections.defaultdict(list)
for e, m, x, y in q("SELECT id, map, x, y FROM ours.spawn"):
    spawns[e].append((m, x, y))

STARTER = {
    'Northshire Valley': (0, -9200, -8700, -500, 100),
    'Coldridge Valley': (0, -6500, -5900, 0, 900),
    'Deathknell': (0, 1600, 2100, 1350, 1800),
    'Shadowglen': (1, 10000, 10900, 550, 1200),
    'Valley of Trials': (1, -900, -200, -4900, -3900),
    'Camp Narache': (1, -3450, -2650, -650, 150),
}

def starter_zone(e):
    for name, (m, x0, x1, y0, y1) in STARTER.items():
        for sm, x, y in spawns.get(e, ()):
            if sm == m and x0 < x < x1 and y0 < y < y1:
                return name
    return None

COLS = ('ItemType', 'Item', 'Chance', 'QuestRequired', 'LootMode', 'GroupId', 'MinCount', 'MaxCount')

def rows(db_, table, entry):
    if db_ == 'ours':
        return [tuple(r) for r in q(f"SELECT ItemType, Item, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment FROM ours.{table} WHERE Entry = %s", entry)]
    return [tuple(r) + ('',) for r in q(f"SELECT ItemType, Item, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount FROM {VM}.{table} WHERE Entry = %s", entry)]

def key(r):
    return (r[0], r[1], f32(r[2]), r[3], r[5], r[6], r[7])

missing70532 = collections.defaultdict(set)   # item -> set of (table, entry)

def target_rows(vm_rows, our_rows, table, entry):
    """vMaNGOS rows (items missing from 70532 dropped) + our Forever-only item rows."""
    out = []
    for r in vm_rows:
        if r[0] == 0 and r[1] not in items70532:
            missing70532[r[1]].add((table, entry))
            continue
        out.append(r[:8] + ('VMaNGOS 1.12',))
    vm_set = set(r[1] for r in vm_rows if r[0] == 0)
    # Forever-only items, and Forever quest drops of vanilla items the VMaNGOS table does not have (e.g. Reagent Pouch 6652 on
    # Stonesplinter Seer for the dwarf shaman's Call of Fire): kept, they belong to Forever quests and need sniffs
    kept = [r for r in our_rows if r[0] == 0 and (r[1] not in vm_items or (r[3] and r[1] not in vm_set))]
    return out + kept, kept

TABLES = {'loot': ('creature_loot_template', 'tc_creature', 2, 1),
          'skin': ('skinning_loot_template', 'tc_skinning', 3, 2),
          'pick': ('pickpocketing_loot_template', 'tc_pickpocket', 4, 3)}

shared = sorted(e for e in vm_ct if e in our_ct)
forever_only = sorted(e for e in our_ct if e not in vm_ct and e in spawns)

changes = {k: {} for k in TABLES}   # table id -> info
id_updates = []                      # (entry, col, old, new)
creature_info = {}
refs_needed = set()

for kind, (table, vmtable, vmcol, ourcol) in TABLES.items():
    done = set()
    for e in shared:
        vid = vm_ct[e][vmcol]
        if not vid:
            continue
        od = our_diff.get(e)
        oid = od[ourcol] if od else None   # None: row comes from the TDB base, not in the repo SQL
        colname = {'loot': 'LootID', 'skin': 'SkinLootID', 'pick': 'PickPocketLootID'}[kind]
        if oid != vid:
            id_updates.append((e, colname, oid, vid))
        if vid in done:
            continue
        done.add(vid)
        vrows = rows(VM, vmtable, vid)
        orows = rows('ours', table, vid)
        tgt, kept = target_rows(vrows, orows, table, vid)
        for r in tgt:
            if r[0] == 1:
                refs_needed.add(r[1])
        if sorted(map(key, tgt)) != sorted(map(key, orows)):
            ok = {key(r): r for r in orows}
            tk = {key(r): r for r in tgt}
            # describe the per-item differences
            byitem_o = collections.defaultdict(list)
            byitem_t = collections.defaultdict(list)
            for r in orows: byitem_o[(r[0], r[1])].append(r)
            for r in tgt: byitem_t[(r[0], r[1])].append(r)
            diffs = []
            for it in sorted(set(byitem_o) | set(byitem_t)):
                a = byitem_o.get(it, []); b = byitem_t.get(it, [])
                if sorted(map(key, a)) == sorted(map(key, b)):
                    continue
                diffs.append({'type': it[0], 'item': it[1],
                              'ours': [[f32(r[2]), r[3], r[5], r[6], r[7], r[8]] for r in a],
                              'vmangos': [[f32(r[2]), r[3], r[5], r[6], r[7]] for r in b]})
            changes[kind][vid] = {'rows': tgt, 'diffs': diffs, 'kept': len(kept), 'creatures': []}
    for e in shared:
        vid = vm_ct[e][vmcol]
        if vid in changes[kind]:
            changes[kind][vid]['creatures'].append(e)

# references, recursively
ref_changes = {}
seen = set()
todo = list(refs_needed)
while todo:
    rid = todo.pop()
    if rid in seen:
        continue
    seen.add(rid)
    vrows = rows(VM, 'tc_reference', rid)
    orows = rows('ours', 'reference_loot_template', rid)
    tgt, kept = target_rows(vrows, orows, 'reference_loot_template', rid)
    for r in tgt:
        if r[0] == 1:
            todo.append(r[1])
    if sorted(map(key, tgt)) != sorted(map(key, orows)):
        ref_changes[rid] = {'rows': tgt, 'ours': len(orows), 'vm': len(tgt)}

# ---------- SQL ----------
def sqlstr(s):
    return "'" + str(s).replace('\\', '\\\\').replace("'", "\\'") + "'"

def fmt(entry, r):
    chs = '%.6g' % f32(r[2])
    return f"({entry},{r[0]},{r[1]},{chs},{r[3]},{r[4]},{r[5]},{r[6]},{r[7]},{sqlstr(r[8] or '')})"

def starter_of_table(info):
    zs = [starter_zone(e) for e in info['creatures']]
    zs = [z for z in zs if z]
    return zs[0] if zs else None

lines = []
w = lines.append
w("-- Classic 1.60: creature loot one-to-one with VMaNGOS 1.12 (core 13b49dc, the snapshot of the world baseline; VMaNGOS has no loot")
w("-- change after it). For every creature that exists in both databases its loot, skinning and pickpocket tables are replaced by the")
w("-- VMaNGOS rows: items, chances, quest flags, groups and counts. Rows of Forever-only items on those tables are kept (their drops")
w("-- need sniffs), items the 70532 client does not have are left out (the server would skip them). Forever-only creatures are not")
w("-- touched. Fixes the sniff import (2026_10_01_04) that set vanilla quest drops to a flat 40% (Tough Wolf Meat and Red Burlap")
w("-- Bandana were 80%, Garrick's head 100%). Report: reports/vmangos-creature-loot.md. Safe to run again.")
w("")

def emit_table(table, tid, info):
    w(f"DELETE FROM `{table}` WHERE `Entry` = {tid};")
    if info['rows']:
        w(f"INSERT INTO `{table}` (`Entry`,`ItemType`,`Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES")
        w(",\n".join(fmt(tid, r) for r in info['rows']) + ";")

order = []
for kind in TABLES:
    for tid, info in changes[kind].items():
        z = starter_of_table(info)
        order.append((0 if z else 1, z or '', kind, tid))
order.sort()

cur_zone = object()
for pri, z, kind, tid in order:
    if z != cur_zone:
        cur_zone = z
        w("")
        w(f"-- {z or 'every other zone the two databases share'}")
    info = changes[kind][tid]
    names = ', '.join(f"{our_ct.get(e, '?')} {e}" for e in info['creatures'][:4]) + (' ...' if len(info['creatures']) > 4 else '')
    w(f"-- {TABLES[kind][0]} {tid}: {names}")
    emit_table(TABLES[kind][0], tid, info)

if ref_changes:
    w("")
    w("-- reference tables those use")
    for rid in sorted(ref_changes):
        emit_table('reference_loot_template', rid, ref_changes[rid])

if id_updates:
    w("")
    w("-- loot table ids of the shared creatures, as in VMaNGOS (a missing difficulty 0 row is left alone)")
    w("DROP TEMPORARY TABLE IF EXISTS `tmp_vmangos_loot_ids`;")
    w("CREATE TEMPORARY TABLE `tmp_vmangos_loot_ids` (`Entry` INT UNSIGNED NOT NULL PRIMARY KEY, `LootID` INT UNSIGNED, `SkinLootID` INT UNSIGNED, `PickPocketLootID` INT UNSIGNED);")
    per = collections.defaultdict(dict)
    for e, col, old, new in id_updates:
        per[e][col] = new
    vals = []
    for e in sorted(per):
        d = per[e]
        vals.append(f"({e},{d.get('LootID', 'NULL')},{d.get('SkinLootID', 'NULL')},{d.get('PickPocketLootID', 'NULL')})")
    for i in range(0, len(vals), 500):
        w("INSERT INTO `tmp_vmangos_loot_ids` VALUES\n" + ",\n".join(vals[i:i + 500]) + ";")
    w("UPDATE `creature_template_difficulty` d JOIN `tmp_vmangos_loot_ids` t ON t.`Entry` = d.`Entry` AND d.`DifficultyID` = 0")
    w("SET d.`LootID` = COALESCE(t.`LootID`, d.`LootID`), d.`SkinLootID` = COALESCE(t.`SkinLootID`, d.`SkinLootID`),")
    w("    d.`PickPocketLootID` = COALESCE(t.`PickPocketLootID`, d.`PickPocketLootID`);")
    w("DROP TEMPORARY TABLE `tmp_vmangos_loot_ids`;")

open(OUT_SQL, 'w').write("\n".join(lines) + "\n")

# ---------- report data ----------
rep = {
    'shared': len(shared),
    'forever_only_spawned': [[e, our_ct[e], sorted(set(m for m, _, _ in spawns[e]))] for e in forever_only],
    'changes': {k: {str(t): {'creatures': [[e, our_ct.get(e)] for e in v['creatures']], 'diffs': v['diffs'], 'kept': v['kept'],
                             'zone': starter_of_table(v)} for t, v in changes[k].items()} for k in TABLES},
    'ref_changes': {str(r): {'ours': v['ours'], 'vm': v['vm']} for r, v in ref_changes.items()},
    'id_updates': id_updates,
    'missing70532': {str(i): [vm_item_name.get(i), sorted(map(list, s))] for i, s in missing70532.items()},
    'starter_creatures': {z: sorted([e, our_ct[e]] for e in shared if starter_zone(e) == z) for z in STARTER},
    'vm_zero_loot_shared_with_ours': [],
}
json.dump(rep, open(OUT_JSON, 'w'), indent=0, default=str)
print('shared', len(shared), 'loot tables changed', {k: len(v) for k, v in changes.items()}, 'refs', len(ref_changes),
      'id updates', len(id_updates), 'missing items', len(missing70532), 'forever-only spawned', len(forever_only))
