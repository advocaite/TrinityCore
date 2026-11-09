# Re-maps the talent / buff spell-modifier effects (ADD_FLAT_MODIFIER 107, ADD_PCT_MODIFIER 108) that the 70291 client reworked, so the VMaNGOS value can be
# taken across: for every effect of a legacy spell whose VMaNGOS row is a spell modifier and whose client effect is a different aura or modifier operation,
# writes the vanilla aura, the vanilla modifier operation (misc value) and the class mask of the spells it affects, as sql/custom/world rows.
#   python3 -I gen_spell_effects.py --audit SpellAudit.tsv --extract data/vmangos_extract.sql.gz [--report effects-report.md] > ../../sql/custom/world/2026_10_12_00_world_classic_spell_effect.sql
# The vanilla spells a modifier affects are the spells of its family whose spellFamilyFlags share a bit with the vanilla mask (spell_affect row, else the
# talent's own spellFamilyFlags). The class mask for the client is the OR of the client family flags of those same spells (SpellAudit.tsv family columns).
# A modifier is written only when the client mask picks no spell the vanilla one does not (see --extras). Safe to run again.
import argparse, collections, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vanilla_spells as v
import compare_spells as cs

MODS = (107, 108)
MAX_VANILLA_OP = 24          # vanilla modifier operations 0..24 are the same numbers in the current client (HealingAndDamage ... BonusCoefficient)


def load_affect(path):
    out = {}
    import gzip
    with (gzip.open(path, 'rt', encoding='utf-8', errors='replace') if path.endswith('.gz') else open(path, encoding='utf-8', errors='replace')) as fh:
        inside = False
        for l in fh:
            if l.startswith('INSERT INTO'):
                inside = l.startswith('INSERT INTO `spell_affect`')
                continue
            if inside:
                m = re.match(r'\t\((\d+), (\d+), (\d+), (\d+), (\d+)\)', l)
                if m and int(m.group(5)) >= 5875 and int(m.group(4)) <= 5875:
                    out[(int(m.group(1)), int(m.group(2)))] = int(m.group(3))
    return out


def split128(x):
    return [(x >> (32 * i)) & 0xFFFFFFFF for i in range(4)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--audit', required=True)
    ap.add_argument('--extract', default=os.path.join(HERE, 'data', 'vmangos_extract.sql.gz'))
    ap.add_argument('--report')
    ap.add_argument('--extras', type=int, default=0, help='most client spells the mask may pick that vanilla did not')
    a = ap.parse_args()
    ix, best = v.load(a.extract)
    affect = load_affect(a.extract)
    audit = cs.read_audit(a.audit)
    num = lambda f, col: int(float(f[ix[col]])) if f[ix[col]].upper() != 'NULL' else 0
    fam = lambda e: (int(audit[e]['family_name']), sum(int(audit[e]['family_%d' % i]) << (32 * i) for i in range(4)))
    by_family = collections.defaultdict(list)       # vanilla family name -> [(spell id, flags)]
    for e, (_, f, _n) in best.items():
        if num(f, 'spellFamilyName'):
            by_family[num(f, 'spellFamilyName')].append((e, int(f[ix['spellFamilyFlags']])))
    client_by_family = collections.defaultdict(list)
    for e in audit:
        if int(audit[e].get('family_name', 0) or 0) and e < 1000000:
            client_by_family[fam(e)[0]].append((e, fam(e)[1]))
    rows, skipped = [], collections.Counter()
    notes = []
    for e in sorted(best):
        if e not in audit or e >= 1000000 or 'family_name' not in audit[e]:
            continue
        _, f, name = best[e]
        for i in (1, 2, 3):
            ve, va = num(f, 'effect%d' % i), num(f, 'effectApplyAuraName%d' % i)
            if ve != 6 or va not in MODS:
                continue
            ce, ca, cm = int(audit[e]['e%d_effect' % i]), int(audit[e]['e%d_aura' % i]), int(audit[e]['e%d_misc' % i])
            vm = num(f, 'effectMiscValue%d' % i)
            if ce == 6 and ca == va and cm == vm:
                continue                                  # the client has the same modifier already
            if vm > MAX_VANILLA_OP:
                skipped['modifier operation %d has no equal in the client' % vm] += 1
                continue
            mask = affect.get((e, i - 1)) or int(f[ix['spellFamilyFlags']])
            familyName = num(f, 'spellFamilyName')
            if not mask or not familyName:
                skipped['no vanilla mask or family'] += 1
                continue
            vanilla_set = {s for s, fl in by_family[familyName] if fl & mask and s in audit and s != e}
            if not vanilla_set:
                skipped['no vanilla spell is affected (or the client lacks them)'] += 1
                continue
            cmask = 0
            for s in vanilla_set:
                if fam(s)[0] == familyName:
                    cmask |= fam(s)[1]
            if not cmask:
                skipped['the client spells of the affected set have no family flags'] += 1
                continue
            picked = {s for s, fl in client_by_family[familyName] if fl & cmask}
            extras = picked - vanilla_set
            if len(extras) > a.extras:
                skipped['client mask would also pick %d+ other spells' % (a.extras + 1)] += 1
                notes.append((e, name, i, sorted(extras)[:6]))
                continue
            rows.append((e, i - 1, va, vm, split128(cmask), name))
    sql = ['-- Classic 1.60: vanilla (VMaNGOS 1.12) spell modifier effects (talents, buffs) the client reworked: aura, modifier operation and the class mask of the spells they affect.',
           '-- Generated by tools/classic_spell_values/gen_spell_effects.py. Safe to run again.',
           'CREATE TABLE IF NOT EXISTS `classic_spell_effect` (',
           '  `spell_id` int unsigned NOT NULL, `effect_index` tinyint unsigned NOT NULL, `apply_aura` smallint unsigned NOT NULL, `misc_value` int NOT NULL,',
           '  `class_mask0` int unsigned NOT NULL, `class_mask1` int unsigned NOT NULL, `class_mask2` int unsigned NOT NULL, `class_mask3` int unsigned NOT NULL,',
           "  `spell_name` varchar(100) NOT NULL DEFAULT '', PRIMARY KEY (`spell_id`,`effect_index`)",
           ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;',
           'DELETE FROM `classic_spell_effect`;']
    vals = ["(%d,%d,%d,%d,%d,%d,%d,%d,'%s')" % (e, i, va, vm, m[0], m[1], m[2], m[3], n.replace("\\'", "'").replace("'", "''")[:100]) for e, i, va, vm, m, n in rows]
    for k in range(0, len(vals), 500):
        sql.append('INSERT INTO `classic_spell_effect` (`spell_id`,`effect_index`,`apply_aura`,`misc_value`,`class_mask0`,`class_mask1`,`class_mask2`,`class_mask3`,`spell_name`) VALUES')
        sql.append(',\n'.join(vals[k:k + 500]) + ';')
    sys.stdout.write('\n'.join(sql) + '\n')
    sys.stderr.write('remapped effects: %d; skipped: %s\n' % (len(rows), dict(skipped)))
    if a.report:
        with open(a.report, 'w', encoding='utf-8') as fh:
            fh.write('# Spell modifier re-mapping\n\nWritten: %d effects. Skipped: %s\n\n## Left out because the client mask would pick other spells\n\n| spell | name | effect | other client spells picked |\n|---|---|---|---|\n' % (len(rows), dict(skipped)))
            for e, n, i, ex in notes:
                fh.write('| %d | %s | %d | %s |\n' % (e, n, i, ', '.join(map(str, ex))))


if __name__ == '__main__':
    main()
