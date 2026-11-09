# Writes the vanilla (VMaNGOS 1.12) cast time, cooldowns, duration and range of the legacy spells whose value differs from this server's, as a
# sql/custom/world file. Inputs: the extract of the VMaNGOS dump (extract-vmangos.ps1), the vanilla SpellCastTimes / SpellDuration / SpellRange DBCs
# (the VMaNGOS data folder) and SpellAudit.tsv (`.playerbot spellaudit` of the server, the client values):
#   python3 -I gen_spell_info.py --audit SpellAudit.tsv --extract data/vmangos_extract.sql.gz --dbc vanilla-dbc \\
#       > ../../sql/custom/world/2026_10_11_05_world_classic_spell_info.sql
# Only the fields that differ are written (NULL = keep the client's), so a spell the client already has right is left alone. Safe to run again.
import argparse, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vanilla_spells as v
import compare_spells as cs

CAST_TOL, TIME_TOL, RANGE_TOL = 0, 0, 0.5
MELEE_RANGE = 5.0      # a vanilla range of 5 yards or less is the melee range, the client keeps its own melee handling
# Party buffs that keep the 70532 duration of their tooltip (1 hour, Battle Shout 3 minutes),
# see 2026_10_24_40_world_classic_party_buff_durations_70532.sql
KEEP_CLIENT_DURATION = {
    19740, 19834, 19835, 19836, 19837, 19838, 25291, 19742, 19850, 19852, 19853, 19854, 25290, 20217, 1038, 19977, 19978, 19979, 25782, 25916, 25894,
    25918, 25898, 25895, 25890, 1126, 5232, 5234, 6756, 8907, 9884, 9885, 1243, 1244, 1245, 2791, 10937, 10938, 14752, 14818, 14819, 27841, 1459, 1460,
    1461, 10156, 10157, 364161, 5242, 6192, 6673, 11549, 11550, 11551, 25289}
# Class spells, talents and pet abilities that keep their 70532 duration, see 2026_10_24_41_world_classic_spell_durations_70532.sql
KEEP_CLIENT_DURATION |= {
    1002, 425728, 425729, 425730, 425732, 425733, 425734, 425735, 425736, 425737, 24423, 24577, 24578, 24579, 24597, 24603, 24604, 24605, 120, 8492,
    10159, 10160, 10161, 20185, 20344, 20345, 20346, 20186, 20354, 20355, 21183, 20188, 20300, 20301, 20302, 20303, 25942, 20050, 6346, 15286, 27827,
    447549, 16886, 14183, 8170, 8184, 10537, 10538, 8227, 8249, 10526, 16387, 8181, 10478, 10479, 8835, 10627, 25359, 5394, 6375, 6377, 10462, 10463,
    5675, 10495, 10496, 10497, 16190, 17354, 17359, 10595, 10600, 10601, 8166, 8071, 8154, 8155, 10406, 10407, 10408, 8075, 8160, 8161, 10442, 25361,
    8143, 8512, 10613, 10614, 15107, 15111, 15112, 17877, 18867, 18868, 18869, 18870, 18871, 18789, 18790, 18791, 18792, 402794, 465352, 1160, 6190,
    11554, 11555, 11556, 2565, 871, 7744}
# Every other spell with a 70532 duration (items, procs, creature spells), see 2026_10_24_42_world_classic_all_durations_70532.sql
KEEP_CLIENT_DURATION |= {
    7230, 7231, 7232, 7233, 7234, 7235, 7236, 7237, 7238, 7239, 7240, 7241, 7242, 7243, 7244, 7245, 7246, 7247, 7248, 7249, 7250, 7251, 7252, 7253, 7254,
    17543, 17544, 17545, 17546, 17548, 17549, 17538, 20587, 15602, 16603, 17196, 17484, 17506, 17499, 17407, 20586, 22639, 22640, 24352, 26066, 13526,
    8313, 17330, 18203, 21952, 18289, 18633, 18652, 17308, 8516, 10608, 10610, 474204, 474268, 14118, 18075, 18200, 18202, 21949, 11790, 12766, 16413,
    18656, 23511, 11841, 435991, 450662, 460756, 462058, 460858, 424, 24734, 24744, 24756, 24758, 24760, 24763, 24765, 24768, 24770, 24772, 24784, 24786,
    24788, 24789, 24790, 8176, 9515, 31726}

# Class spells that keep their 70532 cast time, cooldown, range, mana cost and effect values,
# see 2026_10_25_50_world_classic_spell_timings_costs_values_70532.sql
KEEP_CLIENT_CAST = {
    421, 713, 915, 930, 943, 2860, 6041, 10391, 10392, 10605, 10676, 10688, 10707, 10709, 10711, 10714, 15207, 15208, 19434, 20900, 20901, 20902, 20903,
    20904, 408439, 408440, 408441, 408442, 408443, 408472, 408473, 408475, 408477, 408479, 408481, 408482, 408484}
KEEP_CLIENT_COOLDOWN = {
    355, 633, 871, 1464, 1499, 1510, 2643, 2800, 2944, 3034, 5217, 6343, 6346, 6795, 8198, 8204, 8205, 8820, 10310, 10676, 10688, 10707, 10709, 10711,
    10714, 10797, 11580, 11581, 11604, 11605, 12975, 13795, 13809, 13813, 14279, 14280, 14294, 14295, 14302, 14303, 14304, 14305, 14310, 14311, 14316,
    14317, 15286, 16914, 17364, 17401, 17402, 18708, 19276, 19277, 19278, 19279, 19280, 19296, 19299, 19302, 19303, 19304, 19305, 20230, 20473, 20929,
    20930, 400616, 400618, 400619, 400620, 400621, 400622, 400623, 403677, 403685, 403686, 403687, 403688, 403689, 408479, 408481, 408482, 408484, 409691,
    409693, 409748, 409750, 409751, 409752, 409754, 409755}
KEEP_CLIENT_RANGE = {
    17877, 18867, 18868, 18869, 18870, 18871, 25903, 25913, 25914, 425728, 425729, 425730, 425732, 425733, 425734, 425735, 425736, 425737}
KEEP_CLIENT_COST = {
    421, 678, 679, 680, 915, 930, 943, 1866, 2495, 2643, 2651, 2860, 2973, 3043, 5176, 5177, 5178, 5179, 5180, 5188, 5189, 5569, 6041, 6778, 6780, 6789,
    8903, 8905, 8936, 8938, 8939, 8940, 8941, 9750, 9758, 9856, 9857, 9858, 9888, 9889, 9912, 10332, 10333, 10391, 10392, 10605, 13896, 15207, 15208,
    16190, 17354, 17359, 17364, 17925, 17926, 19271, 19273, 19274, 19275, 19306, 20163, 20419, 20421, 20422, 20423, 24118, 24119, 24120, 25297, 403677,
    403685, 403686, 403687, 403688, 403689, 468766}
KEEP_CLIENT_VALUE = {
    (704, 0), (5221, 0), (7658, 0), (7659, 0), (8909, 2), (11124, 0), (11124, 1), (11717, 0), (13624, 0), (15265, 1), (16415, 0), (16415, 1), (17148, 1),
    (17153, 0), (17504, 0), (18104, 0), (18107, 0), (18211, 0), (18796, 0), (18796, 1), (18832, 0), (18832, 1), (18833, 0), (20187, 0), (20224, 0),
    (20280, 0), (20281, 0), (20282, 0), (20283, 0), (20284, 0), (20285, 0), (20286, 0), (21987, 0), (25713, 0), (25735, 0), (25736, 0), (25737, 0),
    (25738, 0), (25739, 0), (25740, 0), (25741, 0), (25742, 0), (25819, 0), (408423, 0), (408424, 0), (408426, 0), (408427, 0), (408428, 0)}

def esc(n):
    return n.replace("\\'", "'").replace("'", "''")[:100]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--audit', required=True)
    ap.add_argument('--extract', default=os.path.join(HERE, 'data', 'vmangos_extract.sql.gz'))
    ap.add_argument('--dbc', required=True)
    a = ap.parse_args()
    ix, best = v.load(a.extract)
    audit = cs.read_audit(a.audit)
    dbc = cs.load_dbc(a.dbc)
    num = lambda f, col: float(f[ix[col]]) if f[ix[col]].upper() != 'NULL' else 0.0
    rows = []
    for e in sorted(best):
        if e not in audit or e >= 1000000:
            continue
        _, f, name = best[e]
        c = audit[e]
        cast = dbc['cast'].get(int(num(f, 'castingTimeIndex')), 0)
        rec = int(num(f, 'recoveryTime'))
        cat = int(num(f, 'categoryRecoveryTime'))
        dur = dbc['dur'].get(int(num(f, 'durationIndex')))
        rng = dbc['range'].get(int(num(f, 'rangeIndex')), 0.0)
        norm = lambda d: 0 if d is None or d <= 0 else d
        out = {}
        if e not in KEEP_CLIENT_CAST and abs(cast - int(c['cast_ms'])) > CAST_TOL:
            out['cast_ms'] = cast
        if e not in KEEP_CLIENT_COOLDOWN and abs(rec - int(c['recovery_ms'])) > TIME_TOL:
            out['recovery_ms'] = rec
        if e not in KEEP_CLIENT_COOLDOWN and abs(cat - int(c['category_recovery_ms'])) > TIME_TOL:
            out['category_recovery_ms'] = cat
        if dur is not None and e not in KEEP_CLIENT_DURATION and norm(dur) != norm(int(c['duration_ms'])):
            out['duration_ms'] = dur
        if e not in KEEP_CLIENT_RANGE and rng > MELEE_RANGE and abs(rng - float(c['range_max'])) > RANGE_TOL:
            out['range_max'] = rng
        if out:
            rows.append((e, name, out))
    cols = ('cast_ms', 'recovery_ms', 'category_recovery_ms', 'duration_ms', 'range_max')
    sql = ['-- Classic 1.60: vanilla (VMaNGOS 1.12) cast time, cooldowns, duration and range of the legacy spells where they differ from the client data.',
           '-- NULL keeps the client value. Generated by tools/classic_spell_values/gen_spell_info.py. Safe to run again.',
           'CREATE TABLE IF NOT EXISTS `classic_spell_info` (',
           '  `spell_id` int unsigned NOT NULL, `cast_ms` int DEFAULT NULL, `recovery_ms` int DEFAULT NULL, `category_recovery_ms` int DEFAULT NULL,',
           '  `duration_ms` int DEFAULT NULL, `range_max` float DEFAULT NULL, `radius1` float DEFAULT NULL, `radius2` float DEFAULT NULL, `radius3` float DEFAULT NULL,',
           "  `spell_name` varchar(100) NOT NULL DEFAULT '', PRIMARY KEY (`spell_id`)",
           ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;',
           'DELETE FROM `classic_spell_info`;']
    fmt = lambda x: 'NULL' if x is None else (repr(float(x)) if isinstance(x, float) else str(x))
    vals = ["(%d,%s,NULL,NULL,NULL,'%s')" % (e, ','.join(fmt(o.get(k)) for k in cols), esc(n)) for e, n, o in rows]
    for i in range(0, len(vals), 500):
        sql.append('INSERT INTO `classic_spell_info` (`spell_id`,`cast_ms`,`recovery_ms`,`category_recovery_ms`,`duration_ms`,`range_max`,`radius1`,`radius2`,`radius3`,`spell_name`) VALUES')
        sql.append(',\n'.join(vals[i:i + 500]) + ';')
    sys.stdout.write('\n'.join(sql) + '\n')
    counts = {k: sum(1 for _, _, o in rows if k in o) for k in cols}
    sys.stderr.write('spells: %d, fields: %s\n' % (len(rows), counts))


if __name__ == '__main__':
    main()
