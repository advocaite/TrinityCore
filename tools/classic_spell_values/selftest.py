# Runs the generator on a tiny hand-made extract (python3 -I selftest.py): checks the parser, the effect / aura allowlists, the
# per-combo-point and misc-value columns, the mana cost rows and the base mana rows.
import gzip, os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vanilla_spells as v
import gen_spell_values as g

COLS = ['entry', 'build', 'manaCost', 'manaCostPercentage'] \
    + ['effect%d' % i for i in (1, 2, 3)] + ['effectDieSides%d' % i for i in (1, 2, 3)] + ['effectBasePoints%d' % i for i in (1, 2, 3)] \
    + ['effectRealPointsPerLevel%d' % i for i in (1, 2, 3)] + ['effectApplyAuraName%d' % i for i in (1, 2, 3)] \
    + ['effectMiscValue%d' % i for i in (1, 2, 3)] + ['effectPointsPerComboPoint%d' % i for i in (1, 2, 3)] + ['name', 'rank', 'effectBonusCoefficient1', 'effectBonusCoefficient2', 'effectBonusCoefficient3']


def row(entry, build, mana, pct, effects, name, coeff=(-1, -1, -1)):
    # effects: up to 3 of (effect, die, base, ppl, aura, misc, combo)
    e = list(effects) + [(0, 0, 0, 0.0, 0, 0, 0.0)] * (3 - len(effects))
    vals = [entry, build, mana, pct] + [x[0] for x in e] + [x[1] for x in e] + [x[2] for x in e] + [x[3] for x in e] \
        + [x[4] for x in e] + [x[5] for x in e] + [x[6] for x in e]
    return "\t(" + ", ".join(str(x) for x in vals) + ", '" + name + "', 'Rank, 1', " + ", ".join(str(x) for x in coeff) + "),\n"


rows = [
    row(25307, 5875, 380, 0, [(2, 57, 481, 3.1, 0, 0, 0.0)], "Shadow Bolt", (0.857, -1, -1)),
    row(25307, 5000, 999, 0, [(2, 1, 1, 0.0, 0, 0, 0.0)], "Shadow Bolt"),                       # older build, must lose
    row(25307, 9999, 1, 0, [(2, 1, 1, 0.0, 0, 0, 0.0)], "Shadow Bolt"),                         # newer than 1.12, must be ignored
    row(31018, 5875, 0, 0, [(2, 1, 199, 0.0, 0, 0, 25.0)], "Eviscerate"),
    row(1672, 5875, 0, 0, [(58, 0, 0, 0.0, 0, 0, 0.0)], "Heroic Strike"),
    row(17793, 5875, 0, 0, [(6, 1, 99, 0.0, 108, 7, 0.0)], "Ruin"),
    row(980, 5875, 0, 0, [(6, 1, 6, 0.0, 3, 0, 0.0)], "Curse of Agony"),
    row(1000, 5875, 0, 14, [(50, 0, 0, 0.0, 0, 0, 0.0)], "Summon something"),                    # not an allowlisted effect
    row(19943, 5875, 0, 0, [(77, 41, 342, 2.6, 0, 0, 0.0)], "Flash of Light", (0, -1, -1)),      # heals through 19993: its own coefficient is 0
    row(19993, 5875, 0, 0, [(10, 1, 0, 0.0, 0, 0, 0.0)], "Flash of Light", (0.429, -1, -1)),   # the carrier holds the spell power factor
    row(1259652, 5875, 5, 0, [(2, 1, 10, 0.0, 0, 0, 0.0)], "Forever spell"),                    # new to Forever, skipped
]
stats_head = "INSERT INTO `player_classlevelstats` (`class`, `level`, `basehp`, `basemana`, `str`, `agi`, `sta`, `inte`, `spi`) VALUES\n"
text = "INSERT INTO `spell_template` (" + ", ".join("`%s`" % c for c in COLS) + ") VALUES\n" + "".join(rows) \
    + "INSERT INTO `other_table` (`a`) VALUES\n\t(1, 2, 3),\n" + stats_head + "\t(8, 60, 1000, 1213, 30, 30, 30, 120, 100),\n\t(8, 1, 52, 100, 10, 10, 10, 10, 10);\n"

with tempfile.TemporaryDirectory() as d:
    path = os.path.join(d, 'x.sql.gz')
    with gzip.open(path, 'wt', encoding='utf-8') as fh:
        fh.write(text)
    ix, best, stats = v.load_all(path)

assert best[25307][0] == 5875 and best[25307][1][ix['manaCost']] == '380', best[25307]
assert (8, 60) in stats and stats[(8, 60)]['basemana'] == 1213

vals, n = g.gen_values(ix, best)
assert n == 7, (n, vals)      # shadow bolt, eviscerate, heroic strike (weapon damage), ruin, agony, Flash of Light and its carrier; not the summon, not the Forever spell
assert "(19943,0,10,0,342,41,2.6,0,0,0.429,'Flash of Light')" in vals, vals   # the rank takes the carrier's 0.429, not its own 0
assert "(25307,0,2,0,481,57,3.1,0,0,0.857,'Shadow Bolt')" in vals, vals
assert "(31018,0,2,0,199,1,0,0,25.0,-1.0,'Eviscerate')" in vals, vals
assert "(1672,0,58,0,0,0,0,0,0,-1.0,'Heroic Strike')" in vals, vals
assert "(17793,0,6,108,99,1,0,7,0,-1.0,'Ruin')" in vals, vals
assert "(980,0,6,3,6,1,0,0,0,-1.0,'Curse of Agony')" in vals, vals

costs, n = g.gen_costs(ix, best)
assert n == 2 and "(25307,380,0,'Shadow Bolt')" in costs and "(1000,0,14,'Summon something')" in costs, costs

mana, n = g.gen_mana(stats)
assert n == 2 and "SET `basemana` = 1213 WHERE `class` = 8 AND `level` = 60;" in mana, mana
print("ok")
