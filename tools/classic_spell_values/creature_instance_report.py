# Compares the [creature_stats] lines of `.playerbot creaturestats ... instances` taken before and after a change with the VMaNGOS 1.12 creature_template
# row, per instance. Read-only; writes one Markdown report.
#   python3 -I creature_instance_report.py --before creature-before.log --after creature-after.log --extract vmangos_extract.sql.gz --spawns vanilla-instance-spawns.tsv --out report.md
# --spawns is a two column tab file (map id, creature entry) of the creatures spawned on each instance map in the VMaNGOS dump.
import argparse, collections, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare_creatures as c

MAPS = {33: 'Shadowfang Keep', 34: 'The Stockade', 36: 'The Deadmines', 43: 'Wailing Caverns', 47: 'Razorfen Kraul', 48: 'Blackfathom Deeps', 70: 'Uldaman',
        90: 'Gnomeregan', 109: 'Sunken Temple', 129: 'Razorfen Downs', 189: 'Scarlet Monastery', 209: "Zul'Farrak", 229: 'Blackrock Spire', 230: 'Blackrock Depths',
        249: "Onyxia's Lair", 289: 'Scholomance', 329: 'Stratholme', 349: 'Maraudon', 389: 'Ragefire Chasm', 409: 'Molten Core', 429: 'Dire Maul', 469: 'Blackwing Lair',
        509: 'Ruins of Ahn\'Qiraj', 531: 'Temple of Ahn\'Qiraj', 533: 'Naxxramas'}
KV = re.compile(r'(\w+)=("[^"]*"|\S+)')
FIELDS = ('level', 'rank', 'health', 'mana', 'armor', 'dmg', 'atk_ms', 'res')


def read_log(path):
    out = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        if '[creature_stats]' not in line:
            continue
        d = {k: v.strip('"') for k, v in KV.findall(line)}
        if 'entry' in d:
            out[int(d['entry'])] = d
    return out


def vanilla_expect(r):
    # per hit damage of the VMaNGOS row; dmg_multiplier scales it
    m = r.get('dmg_multiplier') or 1.0
    return {'level': (r['level_min'], r['level_max']), 'rank': r['rank'], 'health': (r['health_min'], r['health_max']), 'mana': (r['mana_min'], r['mana_max']),
            'armor': r['armor'], 'dmg': (r['dmg_min'] * m, r['dmg_max'] * m), 'atk_ms': r['base_attack_time'],
            'res': tuple(int(r.get(k) or 0) for k in ('holy_res', 'fire_res', 'nature_res', 'frost_res', 'shadow_res', 'arcane_res'))}


def mismatches(ours, r):
    v = vanilla_expect(r)
    bad = {}
    lv = int(ours['level'])
    if not (v['level'][0] <= lv <= v['level'][1]):
        bad['level'] = '%d vs %d-%d' % (lv, v['level'][0], v['level'][1])
    if int(ours['rank']) != int(v['rank'] or 0):
        bad['rank'] = '%s vs %d' % (ours['rank'], v['rank'] or 0)
    h = int(ours['health'])
    lo, hi = v['health']
    if not (lo * 0.98 - 2 <= h <= hi * 1.02 + 2):
        bad['health'] = '%d vs %d-%d (%+.0f%%)' % (h, lo, hi, 100.0 * (h - (lo + hi) / 2) / max((lo + hi) / 2, 1))
    mn = int(ours['mana'])
    if not (v['mana'][0] * 0.98 - 2 <= mn <= v['mana'][1] * 1.02 + 2):
        bad['mana'] = '%d vs %d-%d' % (mn, v['mana'][0], v['mana'][1])
    ar = float(ours['armor'])
    if abs(ar - v['armor']) > max(2, v['armor'] * 0.02):
        bad['armor'] = '%d vs %d' % (ar, v['armor'])
    dmax = float(ours['dmg_max'])
    if v['dmg'][1] > 0 and abs(dmax - v['dmg'][1]) > max(2, v['dmg'][1] * 0.15):
        bad['damage'] = '%d-%d vs %d-%d' % (float(ours['dmg_min']), dmax, v['dmg'][0], v['dmg'][1])
    if int(ours['atk_ms']) != int(v['atk_ms'] or 0):
        bad['atk_ms'] = '%s vs %d' % (ours['atk_ms'], v['atk_ms'] or 0)
    res = tuple(int(x) for x in ours['res'].split('/'))
    if res != v['res']:
        bad['resist'] = '%s vs %s' % ('/'.join(map(str, res)), '/'.join(map(str, v['res'])))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--before', required=True)
    ap.add_argument('--after', required=True)
    ap.add_argument('--extract', required=True)
    ap.add_argument('--spawns', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    van = c.load_vanilla(a.extract)
    before, after = read_log(a.before), read_log(a.after)
    maps = collections.defaultdict(set)
    for l in open(a.spawns):
        m, e = l.split()
        maps[int(m)].add(int(e))
    out = ['# Creature stats against VMaNGOS 1.12, per instance, before and after the vanilla stats change\n',
           'Ours = the [creature_stats] lines of `creaturestats instances` (before %d entries, after %d). Vanilla = creature_template of the VMaNGOS dump (highest patch <= 10). '
           'Tolerance: health/mana 2%%, armor 2%%, damage max 15%% (per hit, VMaNGOS dmg_multiplier applied), level inside the range, resistances exact (holy/fire/nature/frost/shadow/arcane).\n' % (len(before), len(after))]
    out.append('| instance | creatures read | before: all fields match | after: all fields match | after: health | armor | mana | damage | atk ms | resist | level | rank |')
    out.append('|---|---|---|---|---|---|---|---|---|---|---|---|')
    detail, worst_all = [], []
    tot = collections.Counter()
    for m in sorted(maps):
        ents = sorted(e for e in maps[m] if e in after and e in van)
        if not ents:
            continue
        okb = oka = 0
        fc = collections.Counter()
        rows = []
        for e in ents:
            bb = mismatches(before[e], van[e]) if e in before else {'missing': 'no before line'}
            aa = mismatches(after[e], van[e])
            okb += not bb
            oka += not aa
            for k in aa:
                fc[k] += 1
            if aa:
                rows.append((e, after[e]['name'], aa, bb))
        out.append('| %s (%d) | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |' % (MAPS.get(m, m), m, len(ents), okb, oka, fc['health'], fc['armor'], fc['mana'], fc['damage'], fc['atk_ms'], fc['resist'], fc['level'], fc['rank']))
        tot['n'] += len(ents); tot['before'] += okb; tot['after'] += oka
        if rows:
            detail.append((m, rows))
    out.append('| **all** | %d | %d | %d | | | | | | | | |\n' % (tot['n'], tot['before'], tot['after']))
    not_read = sorted(e for m in maps for e in maps[m] if e not in after)
    out.append('Entries spawned in the dump instances but not read by the server (not in this server\'s creature_template or could not be summoned): %d.\n' % len(set(not_read)))
    out.append('## Still off after the change\n')
    for m, rows in detail:
        out.append('### %s (%d): %d creatures\n\n| entry | name | field: ours vs vanilla (after) | before |\n|---|---|---|---|' % (MAPS.get(m, m), m, len(rows)))
        for e, name, aa, bb in rows[:40]:
            out.append('| %d | %s | %s | %s |' % (e, name, '; '.join('%s %s' % (k, v) for k, v in aa.items()), '; '.join(sorted(bb)) or 'ok'))
        if len(rows) > 40:
            out.append('\n(%d more)' % (len(rows) - 40))
        out.append('')
    open(a.out, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    sys.stderr.write('wrote %s\n' % a.out)


main()
