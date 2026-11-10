# Writes the vanilla armor multipliers of sql/custom/world/2026_10_20_00_world_classic_creature_armor.sql.
# creature_classic_stats.armor_multiplier was 1 for every creature: the VMaNGOS extract of June 2021 (data/vmangos_extract.sql.gz)
# still carries an absolute `armor` per creature_template row, and VMaNGOS moved to creature_classlevelstats.armor x
# creature_template.armor_multiplier with migration 20220610151831, then tuned the multipliers from sniffed hits. The core applies
# the same product (Creature::SelectLevel), so Patchwerk stood at 3625 armor instead of vanilla's 1.5 x 3625.
# This replays every world migration of the VMaNGOS checkout dated after the extract on the extract's rows (row deletes,
# inserts and armor_multiplier updates) and takes, per entry, the highest patch row <= 10 (compare_creatures.load_vanilla).
#   VMANGOS_GIT=/path/to/vmangos python3 -I gen_creature_armor.py > ../../sql/custom/world/2026_10_20_00_world_classic_creature_armor.sql
# VMANGOS_SRC overrides the extract path. Safe to run the output again: it resets every multiplier to 1 first.
import glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_creatures as c

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get('VMANGOS_SRC') or os.path.join(HERE, 'data', 'vmangos_extract.sql.gz')
GIT = os.environ.get('VMANGOS_GIT') or os.path.join(HERE, 'data', 'vmangos')
OURS = os.path.join(HERE, '..', '..', 'sql', 'custom', 'world', '2026_10_11_11_world_classic_creature_stats.sql')
FIRST_MIGRATION = '20210614'   # the extract is world_full_14_june_2021; replaying an applied migration changes nothing
SHOW = (16028, 12118, 11982, 10184, 14834, 15339, 15509, 15928, 15952, 16011)


def statements(text):
    """SQL statements of a migration file, comments dropped, quotes respected."""
    out, cur, i, n = [], [], 0, len(text)
    while i < n:
        ch = text[i]
        if ch == "'":
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2
                    continue
                if text[j] == "'":
                    if j + 1 < n and text[j + 1] == "'":
                        j += 2
                        continue
                    break
                j += 1
            cur.append(text[i:j + 1])
            i = j + 1
        elif ch == '-' and text.startswith('--', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif ch == '/' and text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif ch == ';':
            s = ''.join(cur).strip()
            if s:
                out.append(s)
            cur = []
            i += 1
        else:
            cur.append(ch)
            i += 1
    s = ''.join(cur).strip()
    if s:
        out.append(s)
    return out


def split_top(body, sep=','):
    """Split on sep outside quotes and parentheses."""
    parts, cur, depth, i, n = [], [], 0, 0, len(body)
    while i < n:
        ch = body[i]
        if ch == "'":
            j = i + 1
            while j < n:
                if body[j] == '\\':
                    j += 2
                    continue
                if body[j] == "'":
                    if j + 1 < n and body[j + 1] == "'":
                        j += 2
                        continue
                    break
                j += 1
            cur.append(body[i:j + 1])
            i = j + 1
            continue
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
        i += 1
    parts.append(''.join(cur).strip())
    return parts


def value_rows(values):
    """The (...) groups of a VALUES list, as lists of raw tokens."""
    rows = []
    for part in split_top(values):
        part = part.strip()
        if not (part.startswith('(') and part.endswith(')')):
            raise SystemExit('odd VALUES row: %r' % part[:80])
        rows.append(split_top(part[1:-1]))
    return rows


WHERE_ENTRY = re.compile(r'^`?entry`?\s*=\s*(\d+)$')
WHERE_ENTRY_IN = re.compile(r'^`?entry`?\s+IN\s*\(([\d\s,]+)\)$', re.I)
WHERE_PATCH = re.compile(r'^`?patch`?\s*=\s*(\d+)$')
WHERE_PATCH_IN = re.compile(r'^`?patch`?\s+IN\s*\(([\d\s,]+)\)$', re.I)


def where_rows(cond, state):
    """(entry, patch) keys of state that `WHERE cond` selects; only the entry/patch shapes the migrations use."""
    entries, patches = None, None
    for term in re.split(r'\s*(?:&&|\bAND\b)\s*', cond.strip(), flags=re.I):
        term = term.strip()
        if m := WHERE_ENTRY.match(term):
            entries = {int(m.group(1))}
        elif m := WHERE_ENTRY_IN.match(term):
            entries = {int(x) for x in m.group(1).split(',') if x.strip()}
        elif m := WHERE_PATCH.match(term):
            patches = {int(m.group(1))}
        elif m := WHERE_PATCH_IN.match(term):
            patches = {int(x) for x in m.group(1).split(',') if x.strip()}
        else:
            raise SystemExit('unknown WHERE term: %r' % term)
    if entries is None:
        raise SystemExit('WHERE without entry: %r' % cond)
    return [k for k in state if k[0] in entries and (patches is None or k[1] in patches)]


UPDATE = re.compile(r'^UPDATE\s+`?creature_template`?\s+SET\s*(.*?)\s*WHERE\s+(.*)$', re.I | re.S)
DELETE = re.compile(r'^DELETE\s+FROM\s+`?creature_template`?\s+WHERE\s+(.*)$', re.I | re.S)
INSERT = re.compile(r'^(INSERT|REPLACE)\s+(?:IGNORE\s+)?INTO\s+`?creature_template`?\s*(?:\(([^)]*)\))?\s*VALUES\s*(.*)$', re.I | re.S)
COLUMN_LISTS = []   # (migration file, columns) of every column-listed creature_template insert, for the inserts without one

TOUCH = re.compile(r'^(?:INSERT\s+(?:IGNORE\s+)?INTO|REPLACE\s+INTO|DELETE\s+FROM|UPDATE|TRUNCATE(?:\s+TABLE)?)\s+`?creature_template`?\b', re.I)


def columns_for(fname, count):
    """The table's column order for a column-less insert: the nearest column-listed insert with as many columns."""
    after = [cols for f, cols in COLUMN_LISTS if f >= fname and len(cols) == count]
    before = [cols for f, cols in COLUMN_LISTS if f < fname and len(cols) == count]
    if after:
        return after[0]
    if before:
        return before[-1]
    raise SystemExit('no column list with %d columns for %s' % (count, fname))


def apply(stmt, state, names, fname):
    if m := UPDATE.match(stmt):
        sets = {}
        for a in split_top(m.group(1)):
            col, _, val = a.partition('=')
            sets[col.strip().strip('`').lower()] = val.strip()
        if 'entry' in sets:
            raise SystemExit('migration renumbers rows: %r' % stmt[:120])
        if 'armor_multiplier' not in sets and 'patch' not in sets:
            return
        keys = where_rows(m.group(2), state)
        if 'armor_multiplier' in sets:
            for k in keys:
                state[k] = float(sets['armor_multiplier'])
        if 'patch' in sets:   # the row moves to another patch and keeps its multiplier
            for k in keys:
                state[(k[0], int(sets['patch']))] = state.pop(k)
        return
    if m := DELETE.match(stmt):
        for k in where_rows(m.group(1), state):
            del state[k]
        return
    if m := INSERT.match(stmt):
        rows = value_rows(m.group(3))
        cols = [x.strip().strip('`').lower() for x in m.group(2).split(',')] if m.group(2) else columns_for(fname, len(rows[0]))
        for row in rows:
            if len(row) != len(cols):
                raise SystemExit('column count: %r' % stmt[:120])
            r = dict(zip(cols, row))
            key = (int(r['entry']), int(r.get('patch', '0')))
            state[key] = float(r.get('armor_multiplier', '1'))
            if 'name' in r:
                names[key[0]] = r['name'].strip("'")
        return
    if TOUCH.match(stmt):
        raise SystemExit('unhandled creature_template statement: %r' % stmt[:120])


def load_rows(path):
    """{(entry, patch): 1.0} and names of every creature_template row of the extract."""
    state, names, cols, inside = {}, {}, None, False
    with c._open(path) as fh:
        for line in fh:
            if line.startswith('INSERT INTO'):
                inside = line.startswith('INSERT INTO `creature_template`')
                if inside:
                    cols = re.findall(r'`(\w+)`', line.split('VALUES')[0])[1:]
                continue
            if not inside or not line.startswith('\t('):
                continue
            body = line.strip()
            vals = c._split_row(body[1:body.rindex(')')])
            if len(vals) != len(cols):
                continue
            row = dict(zip(cols, vals))
            key = (int(float(row['entry'])), int(float(row.get('patch', '0') or 0)))
            state[key] = 1.0
            names[key[0]] = row.get('name', '').strip("'")
    return state, names


def our_entries(path):
    out, inside = [], False
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            if line.startswith('INSERT INTO `creature_classic_stats`'):
                inside = True
                continue
            if inside and line.startswith('('):
                out.append(int(line[1:line.index(',')]))
            elif inside and not line.startswith('('):
                inside = False
    return out


def main():
    state, names = load_rows(SRC)
    files = sorted(f for f in glob.glob(os.path.join(GIT, 'sql', 'migrations', '*_world.sql'))
                   if os.path.basename(f) >= FIRST_MIGRATION)
    if not files:
        raise SystemExit('no world migrations under %s' % GIT)
    parsed = []
    for f in files:
        with open(f, encoding='utf-8', errors='replace') as fh:
            parsed.append((os.path.basename(f), statements(fh.read())))
    for fname, stmts in parsed:
        for stmt in stmts:
            if (m := INSERT.match(stmt)) and m.group(2):
                COLUMN_LISTS.append((fname, [x.strip().strip('`').lower() for x in m.group(2).split(',')]))
    for fname, stmts in parsed:
        for stmt in stmts:
            apply(stmt, state, names, fname)
    for key, mult in state.items():
        if not 0 <= mult <= 20:
            raise SystemExit('armor_multiplier %r for %r: columns misread' % (mult, key))
    best = {}
    for (entry, patch), mult in state.items():
        if patch <= c.LAST_PATCH and (entry not in best or best[entry][0] < patch):
            best[entry] = (patch, mult)
    ours = our_entries(OURS)
    by_value, missing = {}, 0
    for e in ours:
        if e not in best:
            missing += 1
            continue
        mult = round(best[e][1], 4)
        if mult != 1:
            by_value.setdefault(mult, []).append(e)
    out = ['-- Classic 1.60: vanilla armor multipliers (VMaNGOS creature_template.armor_multiplier after its migrations up to %s,'
           % os.path.basename(files[-1])[:8],
           '-- highest patch row <= 10). Generated by tools/classic_spell_values/gen_creature_armor.py. Safe to run again.',
           'UPDATE `creature_classic_stats` SET `armor_multiplier`=1;']
    for mult in sorted(by_value):
        entries = by_value[mult]
        for i in range(0, len(entries), 500):
            out.append('UPDATE `creature_classic_stats` SET `armor_multiplier`=%s WHERE `entry` IN (%s);'
                       % (('%g' % mult), ','.join(str(e) for e in entries[i:i + 500])))
    sys.stdout.write('\n'.join(out) + '\n')
    changed = sum(len(v) for v in by_value.values())
    sys.stderr.write('migrations: %d, rows: %d, our creatures: %d, multiplier != 1: %d, gone from VMaNGOS (kept 1): %d\n'
                     % (len(files), len(state), len(ours), changed, missing))
    for e in SHOW:
        if e in best:
            sys.stderr.write('  %d %s: patch %d x%g\n' % (e, names.get(e, '?'), best[e][0], best[e][1]))


main()
