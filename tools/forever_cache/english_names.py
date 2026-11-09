#!/usr/bin/env python3
"""Item SQL from a Russian (or other non-English) hotfix cache, with the English names filled in.

A Russian player's Cache\\ADB\\ruRU\\DBCache.bin has the full Item and ItemSparse records Blizzard sends, but with Russian
texts. This writes those records as our hotfix rows with the real English name in item_sparse and the Russian text in
item_sparse_locale. The English name comes from, in this order:
  1. --questie: Questie's item data (github.com/Questie/QuestieDB, data/Classic/classicItemDB.lua, GPLv3): classic item names
  2. --wowsims: wowsims/forever's item database (github.com/wowsims/forever, assets/database/db.json, MIT): Forever item names
  3. --translations: our own translations (tools/forever_cache/item_translations.tsv), for items no open source names yet.
     Swap a translation for the real name as soon as an English cache or another source has it.
An item with no English name from any of them is left out and listed.

    python3 -I tools/forever_cache/english_names.py --repo . --dbcache <player>/Cache/ADB/ruRU/DBCache.bin --locale ruRU \
        --items 1521,9392 --questie classicItemDB.lua --wowsims db.json --translations tools/forever_cache/item_translations.tsv \
        --first-id 2053000 --last-id 2053999 --out file.sql --report names.tsv
"""
import argparse
import csv
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'full_forever'))
import live_hotfix_sql as L  # noqa: E402

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'client_diff'))
import client_diff  # noqa: E402
import wdc5  # noqa: E402

TEXTS = ('Description', 'Display3', 'Display2', 'Display1', 'Display')


def questie_names(path):
    s = open(path, encoding='utf-8').read()
    return {int(m.group(1)): re.sub(r'\\(.)', r'\1', m.group(2))
            for m in re.finditer(r"^\[(\d+)\] = \{'((?:[^'\\]|\\.)*)'", s, re.M)}


def wowsims_names(path):
    return {x['id']: x['name'] for x in json.load(open(path, encoding='utf-8')).get('items', []) if x.get('name')}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--repo', default='.')
    ap.add_argument('--dbcache', required=True)
    ap.add_argument('--locale', default='ruRU')
    ap.add_argument('--items', required=True)
    ap.add_argument('--questie', default='')
    ap.add_argument('--wowsims', default='')
    ap.add_argument('--translations', default='')
    ap.add_argument('--first-id', type=int, required=True)
    ap.add_argument('--last-id', type=int, required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--report', required=True)
    a = ap.parse_args()
    items = [int(x) for x in a.items.split(',') if x]
    q = questie_names(a.questie) if a.questie else {}
    w = wowsims_names(a.wowsims) if a.wowsims else {}
    tr = {}
    if a.translations:
        for r in csv.DictReader(open(a.translations, encoding='utf-8'), delimiter='\t'):
            tr[int(r['item'])] = r
    sch = wdc5.Schemas(a.repo)
    hashes = L.table_hashes('', ['Item', 'ItemSparse'])
    build, cache = L.read_cache(a.dbcache)
    last = {}
    for push, th, rec, status, data in cache:
        for t in ('Item', 'ItemSparse'):
            if th == hashes[t] and push <= 2000000000 and rec in items:
                last[(t, rec)] = (status, data)
    rows, loc, data_rows, report = {'Item': [], 'ItemSparse': []}, [], [], []
    for rec in items:
        if last.get(('Item', rec), (0,))[0] != 1 or last.get(('ItemSparse', rec), (0,))[0] != 1:
            report.append((rec, '', '', 'left out: no valid record in the cache'))
            continue
        sp = L.decode(sch, 'ItemSparse', last[('ItemSparse', rec)][1], rec)
        if rec in q:
            name, src, desc = q[rec], 'real: Questie classic item data', ''
        elif rec in w:
            name, src, desc = w[rec], 'real: wowsims/forever item data', ''
        elif rec in tr:
            name, src, desc = tr[rec]['english'], 'TRANSLATED by us: swap for the real name', tr[rec].get('english_description', '')
        else:
            report.append((rec, sp['Display'], '', 'left out: no English name'))
            continue
        if sp['Description'] and not desc:
            if rec in tr and tr[rec].get('english_description'):
                desc = tr[rec]['english_description']
                src += '; description TRANSLATED by us'
            else:
                src += '; description left empty in English (no source)'
        loc.append([rec] + [sp[k] for k in TEXTS])
        en = dict(sp)
        for k in ('Display3', 'Display2', 'Display1'):
            en[k] = ''
        en['Display'], en['Description'] = name, desc
        en['VerifiedBuild'] = build
        it = L.decode(sch, 'Item', last[('Item', rec)][1], rec)
        it['VerifiedBuild'] = build
        rows['Item'].append(it)
        rows['ItemSparse'].append(en)
        data_rows += [(hashes['Item'], rec), (hashes['ItemSparse'], rec)]
        report.append((rec, sp['Display'], name, src))
    if a.first_id + len(data_rows) - 1 > a.last_id:
        sys.exit('%d hotfix_data rows do not fit in %d-%d' % (len(data_rows), a.first_id, a.last_id))
    out = []
    for t in ('Item', 'ItemSparse'):
        rs = rows[t]
        if not rs:
            continue
        ht = client_diff.camel_to_snake(t)
        cols = list(rs[0])
        out.append('DELETE FROM `%s` WHERE `ID` IN (%s);' % (ht, ','.join(str(r['ID']) for r in rs)))
        out.append('INSERT INTO `%s` (%s) VALUES\n%s;' % (ht, ','.join('`%s`' % c for c in cols),
                                                       ',\n'.join('(' + ','.join(L.lit(r[c]) for c in cols) + ')' for r in rs)))
    if data_rows:
        out.append('DELETE FROM `hotfix_data` WHERE `Id` BETWEEN %d AND %d;' % (a.first_id, a.last_id))
        out.append('DELETE FROM `hotfix_data` WHERE (`TableHash`,`RecordId`) IN (%s);' % ','.join('(%d,%d)' % x for x in data_rows))
        out.append('INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES\n%s;'
                   % ',\n'.join('(%d,%d,%d,%d,1,%d)' % (a.first_id + i, 1397720000 + a.first_id - 2000000 + i, h, r, build)
                                for i, (h, r) in enumerate(data_rows)))
    if loc:
        lc = L.lit(a.locale)
        out.append('DELETE FROM `item_sparse_locale` WHERE `locale`=%s AND `ID` IN (%s);' % (lc, ','.join(str(r[0]) for r in loc)))
        out.append('INSERT INTO `item_sparse_locale` (`ID`,`locale`,`Description_lang`,`Display3_lang`,`Display2_lang`,`Display1_lang`,'
                   '`Display_lang`,`VerifiedBuild`) VALUES\n%s;' % ',\n'.join(
                       '(%d,%s,%s,%d)' % (r[0], lc, ','.join(L.lit(x) for x in r[1:]), build) for r in loc))
    open(a.out, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    with open(a.report, 'w', encoding='utf-8', newline='\n') as f:
        f.write('item\t%s name\tEnglish name\tsource\n' % a.locale)
        for r in report:
            f.write('\t'.join(map(str, r)) + '\n')
    print('%s: %d items (%d real names, %d translated), %d left out' % (
        a.out, len(rows['Item']), sum(1 for r in report if r[3].startswith('real')),
        sum(1 for r in report if r[3].startswith('TRANSLATED')), sum(1 for r in report if r[3].startswith('left out'))))


if __name__ == '__main__':
    main()
