# Compares what this server holds for every legacy spell (SpellAudit.tsv from `.playerbot spellaudit`) with the VMaNGOS 1.12 spell_template row
# (the extract made by extract-vmangos.ps1, or the whole dump through --extract) and writes a Markdown coverage report.
#   python3 -I compare_spells.py --audit SpellAudit.tsv [--extract data/vmangos_extract.sql.gz] [--dbc /mnt/project-files/research/vanilla-dbc]
#                                [--mismatch Server.log] [--out spell-coverage.md]
# The cast time, duration, range and radius of the vanilla row are indexes into SpellCastTimes / SpellDuration / SpellRange / SpellRadius.dbc (--dbc).
import argparse, collections, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vanilla_spells as v

CLASSIC_EFFECTS = (2, 8, 9, 10, 17, 30, 31, 58, 62, 67, 121)    # kept in step with gen_spell_values.py
AURAS = (3, 8, 13, 22, 24, 29, 34, 52, 53, 54, 55, 57, 69, 71, 79, 85, 99, 107, 108, 124, 134, 135, 136, 137)   # same as gen_spell_values.py
VALUE_TOLERANCE = 1.5


def read_dbc(path):
    d = open(path, 'rb').read()
    magic, rc, fc, rs, ss = struct.unpack('<4s4I', d[:20])
    assert magic == b'WDBC' and rs == fc * 4
    rows = {}
    for i in range(rc):
        raw = d[20 + i * rs:20 + (i + 1) * rs]
        rows[struct.unpack('<I', raw[:4])[0]] = raw
    return rows


def load_dbc(folder):
    out = {}
    if not folder:
        return out
    try:
        out['cast'] = {k: struct.unpack('<iii', r[4:16])[0] for k, r in read_dbc(os.path.join(folder, 'SpellCastTimes.dbc')).items()}
        out['dur'] = {k: struct.unpack('<iii', r[4:16])[0] for k, r in read_dbc(os.path.join(folder, 'SpellDuration.dbc')).items()}
        out['radius'] = {k: struct.unpack('<fff', r[4:16])[2] for k, r in read_dbc(os.path.join(folder, 'SpellRadius.dbc')).items()}
        out['range'] = {k: struct.unpack('<ff', r[4:12])[1] for k, r in read_dbc(os.path.join(folder, 'SpellRange.dbc')).items()}
    except OSError as e:
        sys.stderr.write('dbc not read: %s\n' % e)
        return {}
    return out


def vanilla_mean(bp, die):
    return bp + (1.0 if die <= 1 else (die + 1) / 2.0)


def read_audit(path):
    with open(path, encoding='utf-8', errors='replace') as fh:
        header = fh.readline().rstrip('\n').split('\t')
        rows = {}
        for line in fh:
            f = line.rstrip('\n').split('\t')
            if len(f) != len(header):
                continue
            r = dict(zip(header, f))
            rows[int(r['id'])] = r
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--audit', required=True)
    ap.add_argument('--extract', default=os.path.join(HERE, 'data', 'vmangos_extract.sql.gz'))
    ap.add_argument('--dbc', default='/mnt/project-files/research/vanilla-dbc')
    ap.add_argument('--mismatch')
    ap.add_argument('--out', default='spell-coverage.md')
    a = ap.parse_args()

    ix, best = v.load(a.extract)
    audit = read_audit(a.audit)
    dbc = load_dbc(a.dbc)
    num = lambda f, col: float(f[ix[col]]) if f[ix[col]].upper() != 'NULL' else 0.0

    only_vanilla = sorted(e for e in best if e not in audit)
    only_client = sorted(e for e in audit if e not in best)
    both = sorted(e for e in best if e in audit)
    diffs = collections.defaultdict(list)          # field -> [(spell, name, vanilla, client)]
    eff_count = collections.Counter()              # effects by status
    not_covered = collections.Counter()
    for e in both:
        _, f, name = best[e]
        c = audit[e]

        def cmp(field, vv, cv, tol=0.0):
            if abs(float(vv) - float(cv)) > tol:
                diffs[field].append((e, name, vv, cv))
        cmp('recovery_ms', num(f, 'recoveryTime'), c['recovery_ms'])
        cmp('category_recovery_ms', num(f, 'categoryRecoveryTime'), c['category_recovery_ms'])
        if not (num(f, 'procChance') >= 100 and float(c['proc_chance']) in (0.0, 100.0, 101.0)):      # 101 and 0 both mean always
            cmp('proc_chance', num(f, 'procChance'), c['proc_chance'])
        cmp('proc_charges', num(f, 'procCharges'), c['proc_charges'])
        cmp('stack', num(f, 'stackAmount'), c['stack'])
        cmp('max_targets', num(f, 'maxAffectedTargets'), c['max_targets'])
        cmp('base_level', num(f, 'baseLevel'), c['base_level'])
        cmp('max_level', num(f, 'maxLevel'), c['max_level'])
        cmp('spell_level', num(f, 'spellLevel'), c['spell_level'])
        if dbc:
            cmp('cast_ms', dbc['cast'].get(int(num(f, 'castingTimeIndex')), 0), c['cast_ms'])
            if int(num(f, 'durationIndex')) in dbc['dur']:
                cmp('duration_ms', dbc['dur'][int(num(f, 'durationIndex'))], c['duration_ms'])
            cmp('range_max', dbc['range'].get(int(num(f, 'rangeIndex')), 0.0), c['range_max'], 0.5)
        for i in (1, 2, 3):
            ve, vaura = int(num(f, 'effect%d' % i)), int(num(f, 'effectApplyAuraName%d' % i))
            ce, caura = int(c['e%d_effect' % i]), int(c['e%d_aura' % i])
            if ve == 0 and ce == 0:
                continue
            tag = '%s effect %d' % (name, i)
            if ve != ce or (ve in (6, 27) and vaura != caura):
                diffs['effect_type'].append((e, tag, '%d/%d' % (ve, vaura), '%d/%d' % (ce, caura)))
                eff_count['effect type differs (index reordered or reworked)'] += 1
                continue
            vmean = vanilla_mean(num(f, 'effectBasePoints%d' % i), num(f, 'effectDieSides%d' % i))
            classic = c['e%d_classic' % i] == '1'
            cbase = float(c['e%d_base' % i])
            numeric = ve in CLASSIC_EFFECTS or (ve in (6, 27) and vaura in AURAS)
            if classic:
                eff_count['numeric value taken across (classic_spell_value)'] += 1
                if abs(cbase - vmean) > VALUE_TOLERANCE:
                    diffs['value_client_raw'].append((e, tag, round(vmean, 1), round(cbase, 1)))
            elif numeric:
                if abs(cbase - vmean) > VALUE_TOLERANCE:
                    eff_count['numeric effect with a differing value and NO classic row'] += 1
                    diffs['value_uncovered'].append((e, tag, round(vmean, 1), round(cbase, 1)))
                else:
                    eff_count['numeric effect, client value already equals vanilla'] += 1
            else:
                not_covered[(ve, vaura if ve in (6, 27) else 0)] += 1
                eff_count['non-numeric effect (trigger, summon, stun, dummy, proc aura...), not taken across'] += 1
            if not (num(f, 'effectAmplitude%d' % i) == 0 and float(c['e%d_amp' % i]) <= 1):      # the client holds 1 for no tick
                cmp('tick_ms', num(f, 'effectAmplitude%d' % i), c['e%d_amp' % i])
            cmp('trigger_spell', num(f, 'effectTriggerSpell%d' % i), c['e%d_trigger' % i])
            cmp('chain_targets', num(f, 'effectChainTarget%d' % i), c['e%d_chain' % i])
            if dbc and int(num(f, 'effectRadiusIndex%d' % i)) in dbc['radius']:
                cmp('radius', dbc['radius'][int(num(f, 'effectRadiusIndex%d' % i))], c['e%d_radius' % i], 0.5)

    out = ['# Spell coverage: VMaNGOS 1.12 spell_template against this server\n']
    out.append('Source: VMaNGOS dump world_full_14_june_2021 (row with the highest build <= 5875), SpellAudit.tsv of this server%s.\n' % (', vanilla DBC for cast time, duration, range, radius' if dbc else ' (no DBC given: cast time, duration, range, radius not compared)'))
    out.append('## Totals\n')
    out.append('| | spells |\n|---|---|')
    out.append('| in the VMaNGOS dump | %d |' % len(best))
    out.append('| in this server (id < 1,000,000) | %d |' % len(audit))
    out.append('| in both (compared) | %d |' % len(both))
    out.append('| only in VMaNGOS (the 70291 client does not have them, no impact) | %d |' % len(only_vanilla))
    out.append('| only in the client (retail spells, no vanilla row) | %d |\n' % len(only_client))
    out.append('## Effects of the compared spells\n')
    out.append('| status | effects |\n|---|---|')
    for k, n in eff_count.most_common():
        out.append('| %s | %d |' % (k, n))
    out.append('\n## Differences by field (vanilla vs this server)\n')
    out.append('| field | spells that differ |\n|---|---|')
    for k in sorted(diffs, key=lambda k: -len(diffs[k])):
        out.append('| %s | %d |' % (k, len(diffs[k])))
    out.append('\n`value_client_raw` = the client value differs from vanilla but a classic row replaces it (fine). `value_uncovered` = differs and nothing replaces it (needs a classic row).\n')
    for k in sorted(diffs, key=lambda k: -len(diffs[k])):
        out.append('### %s (first 25 of %d)\n\n| spell | name | vanilla | server |\n|---|---|---|---|' % (k, len(diffs[k])))
        for e, name, vv, cv in diffs[k][:25]:
            out.append('| %d | %s | %s | %s |' % (e, name, vv, cv))
        out.append('')
    out.append('## Effect kinds not taken across (top 15)\n\n| effect/aura | effects |\n|---|---|')
    for (ef, au), n in not_covered.most_common(15):
        out.append('| %d/%d | %d |' % (ef, au, n))
    if a.mismatch and os.path.exists(a.mismatch):
        lines = [l for l in open(a.mismatch, encoding='utf-8', errors='replace') if '[spell_value_mismatch]' in l]
        groups = collections.Counter()
        parsed = [dict(re.findall(r'(\w+)=(\S+)', l)) for l in lines]
        for d in parsed:
            ve, va = int(d.get('vanilla_effect', 0)), int(d.get('vanilla_aura', 0))
            if 'client_effect' not in d:
                groups['the client spell has fewer effects than the vanilla one (reworked spell)'] += 1
            elif ve == 27 and int(d['client_effect']) != 27:
                groups['vanilla persistent area aura (Blizzard, Rain of Fire, Flamestrike...): the client has it as a plain or area effect'] += 1
            elif int(d['client_effect']) != ve:
                groups['different effect type (old/test spells and retail reworks)'] += 1
            elif int(d.get('client_aura', 0)) != va:
                groups['same effect, different aura (talent/buff reworked: spell modifier 107/108 vs crit/AP/stat auras)'] += 1
            else:
                groups['same effect and aura, different spell-modifier operation (misc value)'] += 1
        out.append('\n## Classic spell values that did not match the client effect (%d)\n' % len(lines))
        out.append('| kind | effects |\n|---|---|')
        for k, n in groups.most_common():
            out.append('| %s | %d |' % (k, n))
        out.append('\nAll of these keep the CLIENT value (the take-across skips them on purpose, because the client spell is a different effect). Talents among them (Improved Shield Wall, Improved Renew, Improved Enslave Demon...) therefore still use retail numbers until their spell-modifier operation and affected-spell mask are mapped from the vanilla row.\n')
        out.append('| spell | name | effect index | vanilla effect/aura | client effect/aura |\n|---|---|---|---|---|')
        for l in lines:
            d = dict(re.findall(r'(\w+)=(\S+)', l))
            sid = int(d.get('spell', 0))
            nm = best[sid][2] if sid in best else ''
            out.append('| %s | %s | %s | %s/%s | %s/%s |' % (sid, nm, d.get('effect_index'), d.get('vanilla_effect'), d.get('vanilla_aura'), d.get('client_effect', 'none'), d.get('client_aura', '')))
    open(a.out, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    sys.stderr.write('wrote %s (%d compared, %d fields differ)\n' % (a.out, len(both), len(diffs)))


if __name__ == '__main__':
    main()
