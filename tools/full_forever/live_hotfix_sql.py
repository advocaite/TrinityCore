#!/usr/bin/env python3
"""SQL that gives our server Blizzard's live WoW Forever records for chosen spells or items.

Live Forever changes items and spells by hotfix after a client build ships. The live hotfix cache (DBCache.bin) holds those
records; wowsims/forever (MIT, https://github.com/wowsims/forever) builds its data from the public mirror of it:
https://storage.googleapis.com/raidbots-static/wow/forever/enUS/DBCache.bin

    python3 -I tools/full_forever/live_hotfix_sql.py --repo . --dbcache DBCache.bin --dbc <server/data/dbc/enUS> \
        --spells 13165,14318 --items 1203 --first-id 2001100 --last-id 2001199 --out file.sql

--dbc (optional): the .db2 folder of the same client build as the cache (the table hashes come from it). Records come out as same-ID rows
of our hotfix tables (spell_effect, item_sparse, ...), which the server loads over its DB2 data and sends to clients; Spell.db2
texts go through hotfix_blob (enUS). A record live marks removed or invalid becomes a hotfix_data row with that status. The cache
has to be from the build the server reads: rows decode with that build's layouts (src/server/game/DataStores/DB2Metadata.h).
"""
import argparse
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'client_diff'))
import client_diff  # noqa: E402
import wdc5  # noqa: E402

FT_FMT = {'FT_BYTE': 'b', 'FT_SHORT': 'h', 'FT_INT': 'i', 'FT_LONG': 'q', 'FT_FLOAT': 'f'}
# spell tables keyed by SpellID (Spell and SpellName are keyed by the spell itself)
SPELL_TABLES = ['SpellName', 'SpellMisc', 'SpellEffect', 'SpellAuraOptions', 'SpellAuraRestrictions', 'SpellCastingRequirements',
                'SpellCategories', 'SpellClassOptions', 'SpellCooldowns', 'SpellEquippedItems', 'SpellInterrupts', 'SpellLevels',
                'SpellPower', 'SpellReagents', 'SpellShapeshift', 'SpellTargetRestrictions', 'SpellTotems', 'SpellXSpellVisual',
                'SpellLabel']
ITEM_TABLES = ['Item', 'ItemSparse']


def read_cache(path):
    """[(push, table hash, record id, status, data)] in push order; status 1 valid, 2 removed, 3 invalid, 4 not public."""
    b = open(path, 'rb').read()
    if b[:4] != b'XFTH' or struct.unpack_from('<I', b, 4)[0] != 9:
        sys.exit('%s: not an XFTH v9 hotfix cache' % path)
    build = struct.unpack_from('<I', b, 8)[0]
    pos, out = 44, []
    while pos < len(b):
        pos += 4
        _region, push, _unique, th, rec, size = struct.unpack_from('<iiiIii', b, pos)
        status = b[pos + 24]
        pos += 28
        out.append((push, th, rec, status, b[pos:pos + size]))
        pos += size
    out.sort(key=lambda e: e[0])
    return build, out


_SSTR = [0x486E26EE, 0xDCAA16B3, 0xE1918EEF, 0x202DAFDB, 0x341C7DC7, 0x1C365303, 0x40EF2D37, 0x65FD5E49,
         0xD6057177, 0x904ECE93, 0x1C38024F, 0x98FD323B, 0xE3061AE7, 0xA39B0FA1, 0x9797F25F, 0xE4444563]


def name_hash(name):
    """DB2 table hash = Blizzard's SStrHash of the table name in upper case (same value as in the .db2 header)."""
    seed, shift = 0x7FED7FED, 0xEEEEEEEE
    for c in name.upper().encode():
        seed = ((_SSTR[c >> 4] - _SSTR[c & 0xF]) & 0xFFFFFFFF) ^ ((shift + seed) & 0xFFFFFFFF)
        shift = (c + seed + 33 * shift + 3) & 0xFFFFFFFF
    return seed or 1


def table_hashes(folder, names=()):
    out = {n: name_hash(n) for n in names}
    if not folder:
        return out
    for f in glob.glob(os.path.join(folder, '*.db2')):
        with open(f, 'rb') as fh:
            h = fh.read(160)
        if h[:4] == b'WDC5':
            out[os.path.basename(f)[:-4]] = struct.unpack_from('<I', h, 152)[0]
    return out


def decode(sch, table, data, rec):
    meta = sch.meta[table]
    idname, names = wdc5._flat_names(sch, table)
    row, p = {idname: rec}, 0
    for (t, n, signed), nm in zip(meta['fields'], names):
        for k in range(n):
            if t.startswith('FT_STRING'):
                e = data.index(b'\0', p)
                v, p = data[p:e].decode('utf8', 'replace'), e + 1
            else:
                f = FT_FMT[t]
                if not signed and t != 'FT_FLOAT':
                    f = f.upper()
                v = struct.unpack_from('<' + f, data, p)[0]
                p += struct.calcsize(f)
            row[nm[k]] = v
    if p != len(data):
        raise ValueError('%s %d: %d of %d bytes read' % (table, rec, p, len(data)))
    return row


def lit(v):
    if isinstance(v, str):
        return "'" + v.replace('\\', '\\\\').replace("'", "''").replace('\r', '\\r').replace('\n', '\\n') + "'"
    if isinstance(v, float):
        return str(int(v)) if v == int(v) else repr(float('%.7g' % v))
    return str(v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.')
    ap.add_argument('--dbcache', required=True)
    ap.add_argument('--dbc', default='', help='.db2 folder of the build (optional: table hashes are also computed from the names)')
    ap.add_argument('--spells', default='')
    ap.add_argument('--items', default='')
    ap.add_argument('--records', default='', help='file of "Table id" lines: take exactly these records (any table the server reads), '
                    'e.g. the records a new build added or changed')
    ap.add_argument('--whole-tables', default='', help='take every live record of these tables (e.g. the Trait tables)')
    ap.add_argument('--skip', default='', help='records to leave out, as Table:id,Table:id (e.g. records our own hotfixes own)')
    ap.add_argument('--min-push', type=int, default=0, help='only records from this push on')
    ap.add_argument('--max-push', type=int, default=1000000, help='only records up to this push (the cache also holds per-item shop pushes above 16 million)')
    ap.add_argument('--tables', default='', help='only these tables (default: every spell or item table)')
    ap.add_argument('--statuses', default='1,2,3,4', help='hotfix statuses to take (1 valid, 2 removed, 3 invalid, 4 not public); '
                    '3 and 4 are records live hides from its clients (unreleased or gated content), not changes to them')
    ap.add_argument('--first-id', type=int, required=True)
    ap.add_argument('--last-id', type=int, required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    spells = {int(x) for x in a.spells.split(',') if x}
    items = {int(x) for x in a.items.split(',') if x}
    only = {x for x in a.tables.split(',') if x}
    whole = {x for x in a.whole_tables.split(',') if x}
    records = set()
    if a.records:
        for line in open(a.records):
            if line.split():
                t, r = line.split()[:2]
                records.add((t, int(r)))
    statuses = {int(x) for x in a.statuses.split(',') if x}
    skip = {(x.split(':')[0], int(x.split(':')[1])) for x in a.skip.split(',') if x}
    sch = wdc5.Schemas(a.repo)
    hashes = table_hashes(a.dbc, sch.meta.keys())
    by_hash = {v: k for k, v in hashes.items()}
    build, cache = read_cache(a.dbcache)
    last = {}
    for push, th, rec, status, data in cache:
        t = by_hash.get(th)
        if t:
            last[(t, rec)] = (push, status, data)
    rows, blobs, data_rows = {}, [], []
    for (t, rec), (push, status, data) in sorted(last.items()):
        if only and t not in only and t not in whole and (t, rec) not in records:
            continue
        if not a.min_push <= push <= a.max_push or (t, rec) in skip or status not in statuses:
            continue
        if (t, rec) in records and t != 'Spell':
            pass  # listed record: taken as it is
        elif t in whole and t not in SPELL_TABLES and t != 'Spell':
            pass  # every record, removed ones too (they become hotfix_data rows with live's status)
        elif t == 'Spell':
            if rec not in spells and (t, rec) not in records:
                continue
            if status == 1:
                blobs.append((hashes[t], rec, data))
            data_rows.append((hashes[t], rec, status))
            continue
        elif t in ITEM_TABLES:
            if rec not in items:
                continue
        elif t in SPELL_TABLES:
            if status != 1:
                continue  # a removed SpellEffect row can't be matched to a spell without its record: list it by hand
            r = decode(sch, t, data, rec)
            if (r['ID'] if t == 'SpellName' else r.get('SpellID')) not in spells:
                continue
        else:
            continue
        data_rows.append((hashes[t], rec, status))
        if status == 1:
            r = decode(sch, t, data, rec)
            r['VerifiedBuild'] = build
            rows.setdefault(t, []).append(r)
    if a.first_id + len(data_rows) - 1 > a.last_id:
        sys.exit('%d hotfix_data rows do not fit in %d-%d' % (len(data_rows), a.first_id, a.last_id))
    out = []
    for t, rs in rows.items():
        ht = client_diff.camel_to_snake(t)
        cols = list(rs[0])
        out.append('DELETE FROM `%s` WHERE `ID` IN (%s);' % (ht, ','.join(str(r[cols[0]]) for r in rs)))
        out.append('INSERT INTO `%s` (%s) VALUES\n%s;' % (ht, ','.join('`%s`' % c for c in cols),
                                                       ',\n'.join('(' + ','.join(lit(r[c]) for c in cols) + ')' for r in rs)))
    if blobs:
        out.append('DELETE FROM `hotfix_blob` WHERE `locale`=\'enUS\' AND (`TableHash`,`RecordId`) IN (%s);'
                   % ','.join('(%d,%d)' % (h, r) for h, r, _ in blobs))
        out.append('INSERT INTO `hotfix_blob` (`TableHash`,`RecordId`,`locale`,`Blob`,`VerifiedBuild`) VALUES\n%s;'
                   % ',\n'.join("(%d,%d,'enUS',0x%s,%d)" % (h, r, d.hex(), build) for h, r, d in blobs))
    if data_rows:
        out.append('DELETE FROM `hotfix_data` WHERE `Id` BETWEEN %d AND %d;' % (a.first_id, a.last_id))
        out.append('DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN (%s);'
                   % ','.join('(%d,%d)' % (h, r) for h, r, _ in data_rows))
        out.append('INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES\n%s;'
                   % ',\n'.join('(%d,%d,%d,%d,%d,%d)' % (a.first_id + i, 1397720000 + a.first_id - 2000000 + i, h, r, s, build)
                                for i, (h, r, s) in enumerate(data_rows)))
    open(a.out, 'w').write('\n'.join(out) + '\n')
    print('%s: %d rows in %d tables, %d texts, %d hotfix_data (build %d)'
          % (a.out, sum(map(len, rows.values())), len(rows), len(blobs), len(data_rows), build))


if __name__ == '__main__':
    main()
