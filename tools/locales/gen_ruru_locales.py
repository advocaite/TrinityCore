# Writes sql/custom/world/2026_10_24_40_world_ruru_locales_vmangos.sql: the Russian (ruRU) texts the realm is still missing, taken
# from the VMaNGOS world dump of June 2021 (github.com/brotalnia/database, world_full_14_june_2021.7z, locales_* tables, loc8 = ruRU).
# The 2026_10_10_0* quest catalog files stay the first source; a VMaNGOS text is only used for a field that catalog has no Russian
# for, and the output only overwrites a field whose current value has no non-ASCII letter (empty or English), so Russian already
# in the database (catalog or TDB) is never replaced.
#   python3 -I gen_ruru_locales.py /path/to/world_full_14_june_2021.sql > ../../sql/custom/world/2026_10_24_40_world_ruru_locales_vmangos.sql
# Coverage lines go to stderr.
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORLD = os.path.join(HERE, '..', '..', 'sql', 'custom', 'world')
B1 = os.path.join(WORLD, '2026_09_27_00_world_forever_baseline_01.sql')
B2 = os.path.join(WORLD, '2026_09_27_00_world_forever_baseline_02.sql')
CATALOG = {
    'quest_template_locale': '2026_10_10_04_quest_template_locale_catalog_all_mizuka.sql',
    'quest_offer_reward_locale': '2026_10_10_02_quest_offer_reward_locale_catalog_all_mizuka.sql',
    'quest_request_items_locale': '2026_10_10_03_quest_request_items_locale_catalog_all_mizuka.sql',
}
CLASSES = {'Воин', 'Паладин', 'Охотник', 'Охотница', 'Разбойник', 'Разбойница', 'Жрец', 'Жрица', 'Шаман', 'Шаманка', 'Маг',
           'Чернокнижник', 'Чернокнижница', 'Друид'}
RACES = {'человек', 'дворф', 'дворфийка', 'ночной эльф', 'ночная эльфийка', 'гном', 'гномка', 'орк', 'нежить', 'таурен', 'тауренка',
         'тролль', 'небеснорожденный', 'небеснорожденная'}
CYR = re.compile('[\u0400-\u04FF]')
ESC = {'n': '\n', 'r': '\r', 't': '\t', '0': '\0', '\\': '\\', "'": "'", '"': '"', 'Z': '\x1a'}


def tuples(text):
    """Value lists of a VALUES (...),(...) body; stops at ';' or a keyword (ON DUPLICATE KEY UPDATE)."""
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '(':
            row, cur, i = [], None, i + 1
            while True:
                c = text[i]
                if c == "'":
                    i, buf = i + 1, []
                    while True:
                        c = text[i]
                        if c == '\\':
                            buf.append(ESC.get(text[i + 1], text[i + 1])); i += 2; continue
                        if c == "'":
                            if i + 1 < n and text[i + 1] == "'":
                                buf.append("'"); i += 2; continue
                            i += 1; break
                        buf.append(c); i += 1
                    cur = ''.join(buf)
                elif c == ',':
                    row.append(cur); cur = None; i += 1
                elif c == ')':
                    row.append(cur); i += 1; break
                elif c in ' \r\n\t':
                    i += 1
                else:
                    j = i
                    while text[j] not in ',)':
                        j += 1
                    tok = text[i:j].strip(); cur = None if tok == 'NULL' else tok; i = j
            yield row
        elif c == ';' or c.isalpha():
            return
        else:
            i += 1


def rows(data, table, cols=None):
    """Rows of every INSERT/REPLACE into table as dicts (column list from the statement, else from cols)."""
    for m in re.finditer(r"(?:REPLACE|INSERT(?: IGNORE)?) INTO `%s`(?: \(([^)]*)\))? VALUES\s*" % table, data):
        names = re.findall(r'`(\w+)`', m.group(1) or '') or cols
        s = m.end()
        e = re.search(r";\r?\n", data[s:]).end() + s
        for r in tuples(data[s:e]):
            yield dict(zip(names, r))


def create_cols(data, table):
    m = re.search(r"CREATE TABLE IF NOT EXISTS `%s` \((.*?)\r?\n\) ENGINE" % table, data, re.S)
    return re.findall(r"^\s*`(\w+)`", m.group(1), re.M)


def ru(s):
    return bool(s) and bool(CYR.search(s))


def q(s):
    return "'" + s.replace('\\', '\\\\').replace("'", "''").replace('\r', '\\r').replace('\n', '\\n') + "'"


def guard(col):
    # take the new text only when there is one and the current value is empty or plain ASCII (English); Russian already
    # in the row (catalog or TDB) is kept
    return ("`{0}`=IF(VALUES(`{0}`) IS NOT NULL AND (`{0}` IS NULL OR `{0}`='' OR LENGTH(`{0}`)=CHAR_LENGTH(`{0}`)),"
            "VALUES(`{0}`),`{0}`)").format(col)


def emit(out, table, cols, values, update_cols):
    if not values:
        return
    for k in range(0, len(values), 500):
        out.append('INSERT INTO `%s` (%s) VALUES\n%s\nON DUPLICATE KEY UPDATE %s;' % (
            table, ','.join('`%s`' % c for c in cols), ',\n'.join(values[k:k + 500]), ','.join(guard(c) for c in update_cols)))


def main():
    vm = open(sys.argv[1], encoding='utf-8', errors='replace').read()
    b1 = open(B1, encoding='utf-8').read()
    b2 = open(B2, encoding='utf-8').read()
    log = lambda *a: print(*a, file=sys.stderr)

    # the realm's quests: everything a creature or object starts or ends (the baseline replaces those four tables whole)
    realm = set()
    for t in ('creature_queststarter', 'creature_questender', 'gameobject_queststarter', 'gameobject_questender'):
        for r in rows(b1, t, ['id', 'quest', 'VerifiedBuild']):
            realm.add(int(r['quest']))
    vm_quests = {int(r['entry']): r for r in rows(vm, 'quest_template')}
    ours = {int(r['ID']): r for r in rows(b2, 'quest_template')}
    reward = {int(r['ID']): r.get('RewardText') for r in rows(b1, 'quest_offer_reward')}
    request = {int(r['ID']): r.get('CompletionText') for r in rows(b1, 'quest_request_items')}

    cat = {}
    for t, f in CATALOG.items():
        cat[t] = {int(r['ID']): r for r in rows(open(os.path.join(WORLD, f), encoding='utf-8').read(), t) if r['locale'] == 'ruRU'}
    lq = {int(r['entry']): r for r in rows(vm, 'locales_quest')}

    def english(qid, ours_text, vm_col):
        # the realm's own English text when the baseline has the row, else VMaNGOS's (the realm's vanilla source)
        if ours_text is not None:
            return ours_text
        v = vm_quests.get(qid)
        return v.get(vm_col) if v else None

    out = ['-- ruRU texts from VMaNGOS (world_full_14_june_2021, locales_*.loc8) for what the 2026_10_10 quest catalog has no Russian for.',
           '-- Generated by tools/locales/gen_ruru_locales.py; only fields that are empty or English are overwritten. Safe to run again.',
           'SET NAMES utf8mb4;']

    # quest_template_locale: Title -> LogTitle, Objectives -> LogDescription, Details -> QuestDescription, EndText -> AreaDescription
    fields = (('LogTitle', 'Title'), ('LogDescription', 'Objectives'), ('QuestDescription', 'Details'), ('AreaDescription', 'EndText'))
    vals, stats = [], {f: [0, 0, 0] for f, _ in fields}
    for qid in sorted(realm):
        c = cat['quest_template_locale'].get(qid, {})
        l = lq.get(qid, {})
        row, need = {}, False
        for ours_col, vm_col in fields:
            en = english(qid, ours.get(qid, {}).get(ours_col) if qid in ours else None, vm_col)
            if not en:
                continue
            st = stats[ours_col]; st[0] += 1
            if ru(c.get(ours_col)):
                st[1] += 1
            elif ru(l.get(vm_col + '_loc8')):
                st[2] += 1; row[ours_col] = l[vm_col + '_loc8']; need = True
        if need:
            vals.append('(%d,%s,%s)' % (qid, "'ruRU'", ','.join(q(row[f]) if f in row else 'NULL' for f, _ in fields)))
    for f, (n, a, b) in stats.items():
        log('quest %-16s realm quests with English %4d  catalog ruRU %4d  +VMaNGOS %4d  = %4d (%.0f%%)' % (f, n, a, b, a + b, 100.0 * (a + b) / n))
    # NULL = no VMaNGOS text for that field: a new row leaves it empty (English is shown), an existing row keeps its value
    emit(out, 'quest_template_locale', ['ID', 'locale'] + [f for f, _ in fields], vals, [f for f, _ in fields])

    for table, col, vm_col, english_of in (('quest_offer_reward_locale', 'RewardText', 'OfferRewardText', reward),
                                            ('quest_request_items_locale', 'CompletionText', 'RequestItemsText', request)):
        vals, n, a, b = [], 0, 0, 0
        for qid in sorted(realm):
            en = english(qid, english_of.get(qid), vm_col)
            if not en:
                continue
            n += 1
            if ru(cat[table].get(qid, {}).get(col)):
                a += 1
            elif ru(lq.get(qid, {}).get(vm_col + '_loc8')):
                b += 1; vals.append("(%d,'ruRU',%s)" % (qid, q(lq[qid][vm_col + '_loc8'])))
        log('quest %-16s realm quests with English %4d  catalog ruRU %4d  +VMaNGOS %4d  = %4d (%.0f%%)' % (col, n, a, b, a + b, 100.0 * (a + b) / n))
        emit(out, table, ['ID', 'locale', col], vals, [col])

    # quest_objectives_locale: VMaNGOS ObjectiveText1..4 belong to the realm objective of the same quest with StorageIndex 0..3
    objs = [r for r in rows(b1, 'quest_objectives') if r.get('Description')]
    vals = []
    for o in objs:
        qid, si = int(o['QuestID']), int(o['StorageIndex'])
        t = lq.get(qid, {}).get('ObjectiveText%d_loc8' % (si + 1)) if 0 <= si < 4 else None
        if ru(t):
            vals.append("(%s,'ruRU',%d,%d,%s)" % (o['ID'], qid, si, q(t)))
    log('quest objectives with own text %d, VMaNGOS ruRU %d' % (len(objs), len(vals)))
    emit(out, 'quest_objectives_locale', ['ID', 'locale', 'QuestId', 'StorageIndex', 'Description'], vals, ['Description'])

    # creature names and titles, object names, book pages, gossip options: only where the realm's English matches VMaNGOS's
    # (a Forever rename keeps its English instead of getting the old vanilla translation)
    vm_ct = {int(r['entry']): r for r in rows(vm, 'creature_template')}
    our_ct = {int(r['entry']): r for r in rows(b1, 'creature_template')}
    vals = []
    for r in rows(vm, 'locales_creature', create_cols(vm, 'locales_creature')):
        e = int(r['entry'])
        if not ru(r.get('name_loc8')) or e not in vm_ct:
            continue
        if e in our_ct and (our_ct[e].get('name') or our_ct[e].get('Name')) != vm_ct[e].get('name'):
            continue
        sub = r.get('subname_loc8')
        vals.append("(%d,'ruRU',%s,%s)" % (e, q(r['name_loc8']), q(sub) if ru(sub) else 'NULL'))
    log('creature names ruRU %d' % len(vals))
    emit(out, 'creature_template_locale', ['entry', 'locale', 'Name', 'Title'], vals, ['Name', 'Title'])

    vm_gt = {int(r['entry']): r for r in rows(vm, 'gameobject_template')}
    our_gt = {int(r['entry']): r for r in rows(b1, 'gameobject_template')}
    vals = []
    for r in rows(vm, 'locales_gameobject', create_cols(vm, 'locales_gameobject')):
        e = int(r['entry'])
        if not ru(r.get('name_loc8')) or e not in vm_gt:
            continue
        if e in our_gt and our_gt[e].get('name') != vm_gt[e].get('name'):
            continue
        vals.append("(%d,'ruRU',%s)" % (e, q(r['name_loc8'])))
    log('object names ruRU %d' % len(vals))
    emit(out, 'gameobject_template_locale', ['entry', 'locale', 'name'], vals, ['name'])

    vals = []
    for r in rows(vm, 'locales_page_text', create_cols(vm, 'locales_page_text')):
        if ru(r.get('Text_loc8')):
            vals.append("(%s,'ruRU',%s)" % (r['entry'], q(r['Text_loc8'])))
    log('book pages ruRU %d' % len(vals))
    emit(out, 'page_text_locale', ['ID', 'locale', 'Text'], vals, ['Text'])

    vm_go = {(int(r['menu_id']), int(r['id'])): r for r in rows(vm, 'gossip_menu_option')}
    our_go = {(int(r['MenuID']), int(r['OptionID'])): r for r in rows(b1, 'gossip_menu_option')}
    vals = []
    for r in rows(vm, 'locales_gossip_menu_option', create_cols(vm, 'locales_gossip_menu_option')):
        k = (int(r['menu_id']), int(r['id']))
        if not ru(r.get('option_text_loc8')) or k not in our_go or k not in vm_go:
            continue
        if (our_go[k].get('OptionText') or '').strip() != (vm_go[k].get('option_text') or '').strip():
            continue
        box = r.get('box_text_loc8')
        vals.append("(%d,%d,'ruRU',%s,%s)" % (k[0], k[1], q(r['option_text_loc8']), q(box) if ru(box) else 'NULL'))
    log('gossip options ruRU %d (of %d realm options)' % (len(vals), len(our_go)))
    emit(out, 'gossip_menu_option_locale', ['MenuID', 'OptionID', 'Locale', 'OptionText', 'BoxText'], vals, ['OptionText', 'BoxText'])

    # The catalog's Russian was captured with one character's class and race already filled into the declension tokens
    # (|3-6(Воин) where the text means |3-6($C)), so every class saw "warrior". Put the tokens back; the catalog's own
    # correct rows use |3-N($C) and |3-N($R) the same way.
    targets = (('quest_template_locale', ('LogTitle', 'LogDescription', 'QuestDescription', 'AreaDescription', 'QuestCompletionLog')),
               ('quest_offer_reward_locale', ('RewardText',)), ('quest_request_items_locale', ('CompletionText',)),
               ('quest_objectives_locale', ('Description',)))
    texts = [v for t in CATALOG for r in cat[t].values() for k, v in r.items() if k not in ('ID', 'locale') and v]
    texts += [r['Description'] for r in rows(open(os.path.join(WORLD, '2026_10_10_01_quest_objectives_locale_catalog_all_mizuka.sql'),
                                                    encoding='utf-8').read(), 'quest_objectives_locale') if r['locale'] == 'ruRU' and r.get('Description')]
    texts += [v for v in out if v.startswith('(')]
    baked = {}
    for t in texts:
        for m in re.finditer(r'\|3-(\d)\(([^)$%]+)\)', t):
            word = m.group(2)
            token = '$C' if word in CLASSES else '$R' if word.lower() in RACES else None
            if token:
                baked[m.group(0)] = '|3-%s(%s)' % (m.group(1), token)
            else:
                log('declension token left as is: %s' % m.group(0))
    log('baked class/race declensions fixed: %d forms' % len(baked))
    for table, cols in targets:
        sets = []
        for c in cols:
            expr = '`%s`' % c
            for old, new in sorted(baked.items()):
                expr = 'REPLACE(%s,%s,%s)' % (expr, q(old), q(new))
            sets.append('`%s`=%s' % (c, expr))
        out.append("UPDATE `%s` SET %s WHERE `locale`='ruRU';" % (table, ','.join(sets)))

    sys.stdout.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
