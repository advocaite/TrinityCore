# Loads the VMaNGOS-derived data the tactics generator needs.
#   * creature_template and creature (spawns) from the VMaNGOS world dump of 2 October 2017 (path in DUMP; spell_template is not in it).
#   * the creature AI the Forever server actually runs: smart_scripts rows of sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql
#     (generated from the VMaNGOS June 2021 EventAI + spell lists).
#   * damage spell names from sql/custom/world/2026_10_10_01_world_classic_spell_values.sql.
import os, re, collections

DUMP = os.environ.get('VMANGOS_DUMP', '/path/to/vmdb2/dump17/world_full_02_october_2017.sql')
REPO = os.environ.get('FOREVER_REPO', '/path/to/wow-foverver')
SMART = os.path.join(REPO, 'sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql')
SPELLVAL = os.path.join(REPO, 'sql/custom/world/2026_10_10_01_world_classic_spell_values.sql')

def split_row(s):
    """One '(a, b, 'c d', NULL)' line body -> list of python values (ints, floats, str, None)."""
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c in ' ,\t':
            i += 1
        elif c == "'":
            j, buf = i + 1, []
            while j < n:
                if s[j] == '\\' and j + 1 < n:
                    buf.append(s[j + 1]); j += 2
                elif s[j] == "'" and j + 1 < n and s[j + 1] == "'":
                    buf.append("'"); j += 2
                elif s[j] == "'":
                    break
                else:
                    buf.append(s[j]); j += 1
            out.append(''.join(buf)); i = j + 1
        else:
            j = i
            while j < n and s[j] not in ',':
                j += 1
            t = s[i:j].strip(); i = j
            if t.upper() == 'NULL': out.append(None)
            else:
                try: out.append(int(t))
                except ValueError:
                    try: out.append(float(t))
                    except ValueError: out.append(t)
    return out

def table_rows(path, table):
    """Yields dict rows of `table` from a mysqldump file / the smartai sql (INSERT INTO ... (cols) VALUES then one row per line)."""
    cols = None
    pat = re.compile(r'INSERT INTO (?:`?\w+`?\.)?`?%s`? \(([^)]*)\) VALUES' % re.escape(table))
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if cols is None or line.startswith('INSERT'):
                m = pat.match(line)
                if m:
                    cols = [c.strip(' `') for c in m.group(1).split(',')]
                    continue
                if line.startswith('INSERT'):
                    cols = None
                    continue
            if cols is None:
                continue
            t = line.strip()
            if not t.startswith('('):
                if t and not t.startswith('--'):
                    cols = None
                continue
            body = t.rstrip(',;')
            if body.endswith(')'): body = body[1:-1]
            yield dict(zip(cols, split_row(body)))
            if t.endswith(';'):
                cols = None

def load_templates():
    tpl = {}
    for r in table_rows(DUMP, 'creature_template'):
        if r['patch'] == 0 or r['entry'] not in tpl:
            tpl[r['entry']] = r
    return tpl

def load_spawns():
    by_map = collections.defaultdict(collections.Counter)
    for r in table_rows(DUMP, 'creature'):
        by_map[r['map']][r['id']] += 1
    return by_map

def load_smart():
    scripts = collections.defaultdict(list)
    for r in table_rows(SMART, 'smart_scripts'):
        scripts[(r['source_type'], r['entryorguid'])].append(r)
    return scripts

def load_spell_names():
    names = {}
    for r in table_rows(SPELLVAL, 'classic_spell_value'):
        names[r['spell_id']] = r['spell_name']
    return names

def load_ai_comment_names():
    """spell id -> {name: count} from the EventAI comments of the 2017 dump ('Defias Evoker - Cast Flamestrike on ...')."""
    import re
    names = collections.defaultdict(collections.Counter)
    pat = re.compile(r' - Cast (.+?)(?: on | at | when | after | in | if | to | every |$)')
    for r in table_rows(DUMP, 'creature_ai_scripts'):
        m = pat.search(r.get('comment') or '')
        if not m: continue
        nm = m.group(1).strip()
        for k in (1, 2, 3):
            if r.get('action%d_type' % k) == 11 and r.get('action%d_param1' % k):
                names[r['action%d_param1' % k]][nm] += 1
    return names
