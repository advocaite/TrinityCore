# Parses the VMaNGOS dump rows the generator needs: per spell the row with the highest build <= 5875 (patch 1.12), numeric columns up to
# `name`; per class and level the player_classlevelstats row. SRC is either the whole dump (world_full_14_june_2021.sql) or the extract
# made by extract-vmangos.ps1 (data/vmangos_extract.sql.gz: only the INSERT lines of the two tables). Override with VMANGOS_SRC.
import gzip, os, re

TOKEN = re.compile(r"\s*(?:'(?:[^'\\]|\\.|'')*'|[^,]+)")

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get('VMANGOS_SRC') or os.path.join(HERE, 'data', 'vmangos_extract.sql.gz')


def _open(path):
    if path.endswith('.gz'):
        return gzip.open(path, 'rt', encoding='utf-8', errors='replace')
    return open(path, encoding='utf-8', errors='replace')


def _columns(line):
    return re.findall(r'`(\w+)`', line.split('VALUES')[0])[1:]


def load(path=None):
    """Returns (ix, best): column index of spell_template and {entry: (build, fields, name)}."""
    ix, best, _ = load_all(path)
    return ix, best


def load_all(path=None):
    """Returns (ix, best, stats): stats is {(class, level): {column: int}} of player_classlevelstats."""
    cols = None
    ix = {}
    nname = 0
    best = {}
    stats = {}
    statcols = None
    table = None
    with _open(path or SRC) as fh:
        for l in fh:
            if l.startswith('INSERT INTO'):
                if l.startswith('INSERT INTO `spell_template`'):
                    table = 'spell'
                    if cols is None:
                        cols = _columns(l)
                        ix = {c: i for i, c in enumerate(cols)}
                        nname = ix['name']
                elif l.startswith('INSERT INTO `player_classlevelstats`'):
                    table = 'stats'
                    statcols = _columns(l)
                else:
                    table = None
                continue
            if table is None:
                continue
            if table == 'stats':
                m = re.match(r"\t\(([^)]*)\)", l)
                if m:
                    vals = [int(x) for x in m.group(1).replace(' ', '').split(',')]
                    row = dict(zip(statcols, vals))
                    stats[(row['class'], row['level'])] = row
                continue
            m = re.match(r"\t\((\d+), (\d+),", l)
            if not m:
                continue
            e, b = int(m.group(1)), int(m.group(2))
            if b > 5875:
                continue
            if e not in best or b > best[e][0]:
                body = l.rstrip().rstrip(',;')
                body = body[body.index('(') + 1:body.rindex(')')]
                f = [t.strip() for t in TOKEN.findall(body)]
                nm = f[nname]
                if nm.startswith("'") and nm.endswith("'"):
                    nm = nm[1:-1]
                best[e] = (b, f, nm)
    return ix, best, stats
