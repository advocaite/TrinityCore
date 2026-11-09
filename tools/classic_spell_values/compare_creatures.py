# Compares what the server uses for a creature (the [creature_stats] log lines of a run) with the vanilla row of the VMaNGOS dump
# (creature_template in data/vmangos_extract.sql.gz, highest patch row up to 10 = 1.12). Read-only; writes one Markdown table.
#   python3 -I compare_creatures.py --ours Playerbot.log [--extract data/vmangos_extract.sql.gz] [--out creature-stats-vs-vanilla.md]
# A log line looks like
#   [creature_stats] entry=12118 name="Lucifron" level=63 rank=3 health=1560000 mana=0 armor=3600 dmg_min=900 dmg_max=1300 atk_ms=2000 res=0/0/0/0/0/0
# (res is holy/fire/nature/frost/shadow/arcane). Rows are CONFIRMED when both sides were read, INFERRED otherwise.
import argparse, gzip, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOKEN = re.compile(r"\s*(?:'(?:[^'\\]|\\.|'')*'|[^,]+)")
KV = re.compile(r'(\w+)=("[^"]*"|\S+)')
LAST_PATCH = 10
TOLERANCE = 0.10


def _open(path):
    if path.endswith('.gz'):
        return gzip.open(path, 'rt', encoding='utf-8', errors='replace')
    return open(path, encoding='utf-8', errors='replace')


def _num(tok):
    tok = tok.strip()
    if tok.upper() == 'NULL' or tok.startswith("'"):
        return None
    try:
        return float(tok)
    except ValueError:
        return None


def _split_row(body):
    toks = TOKEN.findall(body)
    return [t.strip() for t in toks]


def load_vanilla(path):
    """{entry: {column: value}} of the highest patch row <= LAST_PATCH."""
    cols = None
    out = {}
    inside = False
    with _open(path) as fh:
        for line in fh:
            if line.startswith('INSERT INTO'):
                inside = line.startswith('INSERT INTO `creature_template`')
                if inside:
                    cols = re.findall(r'`(\w+)`', line.split('VALUES')[0])[1:]
                continue
            if not inside or not line.startswith('\t('):
                continue
            body = line.strip()
            body = body[1:body.rindex(')')]
            vals = _split_row(body)
            if len(vals) != len(cols):
                continue
            row = dict(zip(cols, vals))
            entry = int(float(row['entry']))
            patch = int(float(row.get('patch', '0') or 0))
            if patch > LAST_PATCH:
                continue
            if entry in out and out[entry]['_patch'] > patch:
                continue
            r = {k: _num(v) for k, v in row.items()}
            r['name'] = row.get('name', '').strip("'")
            r['_patch'] = patch
            out[entry] = r
    return out


def load_ours(path):
    """{entry: {key: value}} of the last [creature_stats] line per entry."""
    out = {}
    with _open(path) as fh:
        for line in fh:
            if '[creature_stats]' not in line:
                continue
            d = {k: v.strip('"') for k, v in KV.findall(line)}
            if 'entry' not in d:
                continue
            d['entry'] = int(d['entry'])
            for k in ('level', 'rank', 'health', 'mana', 'armor', 'dmg_min', 'dmg_max', 'atk_ms'):
                if k in d:
                    d[k] = float(d[k])
            d['res'] = [float(x) for x in d.get('res', '0/0/0/0/0/0').split('/')]
            out[d['entry']] = d
    return out


def vanilla_numbers(v):
    """The vanilla side in the same terms as the log line."""
    hp_mult = v.get('health_multiplier') or 1.0
    dmg_mult = v.get('damage_multiplier') or v.get('dmg_multiplier') or 1.0
    arm_mult = v.get('armor_multiplier') or 1.0
    def avg(a, b):
        return None if a is None or b is None else (a + b) / 2.0
    health = avg(v.get('health_min'), v.get('health_max'))
    mana = avg(v.get('mana_min'), v.get('mana_max'))
    return {
        'level': (v.get('level_min'), v.get('level_max')),
        'health': None if health is None else health * hp_mult,
        'mana': mana,
        'armor': None if v.get('armor') is None else v['armor'] * arm_mult,
        'dmg_min': None if v.get('dmg_min') is None else v['dmg_min'] * dmg_mult,
        'dmg_max': None if v.get('dmg_max') is None else v['dmg_max'] * dmg_mult,
        'atk_ms': v.get('base_attack_time'),
        'res': [v.get('resistance%d' % i) for i in range(1, 7)],
    }


def ratio(ours, van):
    if ours is None or van is None:
        return None
    if van == 0:
        return None if ours == 0 else float('inf')
    return ours / van


def fmt_ratio(r):
    if r is None:
        return '-'
    if r == float('inf'):
        return 'inf'
    mark = '' if abs(r - 1.0) <= TOLERANCE else (' HIGH' if r > 1 else ' LOW')
    return '%.2f%s' % (r, mark)


def build(ours, vanilla):
    lines = ['| entry | name | level (ours / vanilla) | health | mana | armor | damage per hit (ours / vanilla) | swing ms (ours / vanilla) | resistances differ | status |',
             '|---|---|---|---|---|---|---|---|---|---|']
    flagged = []
    for entry in sorted(ours):
        o = ours[entry]
        v = vanilla.get(entry)
        if v is None:
            lines.append('| %d | %s | %s | - | - | - | - | - | - | INFERRED (no vanilla row) |' % (entry, o.get('name', ''), int(o.get('level', 0))))
            continue
        n = vanilla_numbers(v)
        lv = n['level']
        lvtxt = '%d / %s' % (o.get('level', 0), ('%d' % lv[0]) if lv[0] == lv[1] else ('%d-%d' % (lv[0] or 0, lv[1] or 0)))
        rh, rm, ra = ratio(o.get('health'), n['health']), ratio(o.get('mana'), n['mana']), ratio(o.get('armor'), n['armor'])
        dmg_o = '%d-%d' % (o.get('dmg_min', 0), o.get('dmg_max', 0))
        dmg_v = '-' if n['dmg_min'] is None else '%d-%d' % (n['dmg_min'], n['dmg_max'])
        rd = ratio((o.get('dmg_min', 0) + o.get('dmg_max', 0)) / 2.0, None if n['dmg_min'] is None else (n['dmg_min'] + n['dmg_max']) / 2.0)
        resdiff = [i for i, (a, b) in enumerate(zip(o['res'], n['res'])) if b is not None and abs(a - b) > 0.5]
        names = ['holy', 'fire', 'nature', 'frost', 'shadow', 'arcane']
        restxt = ', '.join('%s %d/%d' % (names[i], o['res'][i], n['res'][i]) for i in resdiff) or '-'
        lines.append('| %d | %s | %s | %s | %s | %s | %s / %s (x%s) | %d / %s | %s | CONFIRMED |' % (
            entry, o.get('name', v.get('name', '')), lvtxt, fmt_ratio(rh), fmt_ratio(rm), fmt_ratio(ra), dmg_o, dmg_v, fmt_ratio(rd),
            o.get('atk_ms', 0), '-' if n['atk_ms'] is None else '%d' % n['atk_ms'], restxt))
        for label, r in (('health', rh), ('mana', rm), ('armor', ra), ('damage', rd)):
            if r is not None and r != float('inf') and abs(r - 1.0) > TOLERANCE:
                flagged.append((entry, o.get('name', ''), label, r))
    return lines, flagged


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ours', required=True)
    ap.add_argument('--extract', default=os.path.join(HERE, 'data', 'vmangos_extract.sql.gz'))
    ap.add_argument('--out', default='creature-stats-vs-vanilla.md')
    a = ap.parse_args()
    vanilla = load_vanilla(a.extract)
    ours = load_ours(a.ours)
    table, flagged = build(ours, vanilla)
    text = ['# Creature stats: server vs VMaNGOS 1.12', '',
            'Ratios are server / vanilla; HIGH / LOW marks more than %d%% off. Vanilla: creature_template, highest patch row up to %d, health x health_multiplier, damage x damage_multiplier. %d creatures read from the log, %d vanilla rows loaded.' % (TOLERANCE * 100, LAST_PATCH, len(ours), len(vanilla)),
            ''] + table + ['', '## Off by more than %d%%' % (TOLERANCE * 100), '']
    for entry, name, label, r in sorted(flagged, key=lambda x: -abs(x[3] - 1.0)):
        text.append('- %d %s: %s x%.2f' % (entry, name, label, r))
    if not flagged:
        text.append('- none')
    with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(text) + '\n')
    print('wrote %s (%d rows, %d flagged)' % (a.out, len(ours), len(flagged)))


if __name__ == '__main__':
    sys.exit(main())
