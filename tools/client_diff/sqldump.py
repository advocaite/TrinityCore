"""Load chosen tables from a mysqldump (.sql or .sql.gz) into SQLite, without a MySQL server.

    con = load_dump('world.sql.gz', {'quest_template', 'trainer_spell'}, sqlite3.connect('w.db'))
"""
import gzip
import re

TOKEN = re.compile(r"'((?:[^'\\]|\\.)*)'|(NULL)|(-?[0-9.]+(?:[eE][-+]?[0-9]+)?)|(0x[0-9A-Fa-f]+)|(\()|(\))")
ESC = {'0': '\0', 'n': '\n', 'r': '\r', 't': '\t', 'Z': '\x1a', 'b': '\b'}


def _unesc(s):
    if '\\' not in s:
        return s
    return re.sub(r'\\(.)', lambda m: ESC.get(m.group(1), m.group(1)), s)


def _rows(line):
    row, depth = None, 0
    for m in TOKEN.finditer(line, line.index(' VALUES ') + 8):
        s, null, num, hx, op, cl = m.groups()
        if op:
            row = []
        elif cl:
            yield row
            row = None
        elif row is None:
            continue
        elif s is not None:
            row.append(_unesc(s))
        elif null:
            row.append(None)
        elif num is not None:
            if '.' in num or 'e' in num or 'E' in num:
                row.append(float(num))
            else:
                v = int(num)
                row.append(v if -(1 << 63) <= v < (1 << 63) else v - (1 << 64))
        elif hx:
            row.append(hx)


def load_dump(path, tables, con, prefix=''):
    op = gzip.open if path.endswith('.gz') else open
    cols, cur = {}, None
    with op(path, 'rt', encoding='utf8', errors='replace') as f:
        for line in f:
            if line.startswith('CREATE TABLE `'):
                cur = line.split('`')[1]
                cols[cur] = []
                continue
            if cur and line.startswith('  `'):
                cols[cur].append(line.split('`')[1])
                continue
            if cur and line.startswith(')'):
                if cur in tables:
                    con.execute('DROP TABLE IF EXISTS "%s%s"' % (prefix, cur))
                    con.execute('CREATE TABLE "%s%s" (%s)' % (prefix, cur, ','.join('"%s"' % c for c in cols[cur])))
                cur = None
                continue
            if line.startswith('INSERT INTO `'):
                t = line.split('`')[1]
                if t not in tables:
                    continue
                n = len(cols[t])
                q = 'INSERT INTO "%s%s" VALUES (%s)' % (prefix, t, ','.join('?' * n))
                con.executemany(q, (r for r in _rows(line) if len(r) == n))
    con.commit()
    return {t: cols.get(t, []) for t in tables}
