#!/usr/bin/env python3
"""Compare players' cache data (JSON from wdb.py, one or more files) with our world database and write a report plus SQL drafts.

    python3 -I tools/forever_cache/compare.py cache1.json [cache2.json ...] --db world --outdir <folder> [--min-caches 1]

Reads the world database with the mysql command line client (read only). Writes into --outdir:
  quests.tsv              every cached quest: missing / placeholder / differs / same, and in how many caches it was seen
  creatures.tsv, gameobjects.tsv   cached NPCs/objects our world has no template for, or whose name or model differ
  placeholder_quests.sql  real text, objectives and rewards for our placeholder quests (quest_template.VerifiedBuild = -1)
  quest_locales.sql       quest texts in other languages (ruRU, ...) where our quest_template_locale row is missing or empty
  gameobjects_missing.sql gameobject_template rows for cached objects we lack (template only; no spawns)
  missing_quests.json     full cached data of quests our world lacks, for the quest builder (givers come from Questie)
A cache from a player who also plays on our realm holds our own answers too, so only quests that are missing or placeholders on
our side are written as SQL; differences on quests we already have are listed, never changed automatically.
Nothing is applied. Every SQL file is safe to run twice.
"""
import argparse
import collections
import json
import os
import subprocess


def db(name, sql):
    o = subprocess.run(['mysql', '-N', '--default-character-set=utf8mb4', name, '-e', sql], capture_output=True, text=True, check=True).stdout
    return [l.split('\t') for l in o.splitlines()]


def esc(s):
    return "'" + str(s).replace('\\', '\\\\').replace("'", "''").replace('\n', '\\n').replace('\r', '') + "'"


def merge(paths, kind):
    """{locale: {id: record}} over every cache file; a record seen in more caches wins ties by count, the newest build otherwise."""
    seen = collections.defaultdict(lambda: collections.defaultdict(list))
    for p in paths:
        d = json.load(open(p, encoding='utf-8'))
        for loc, recs in d.get(kind, {}).items():
            for rid, rec in recs.items():
                seen[loc][int(rid)].append(rec)
    out = {}
    for loc, recs in seen.items():
        out[loc] = {}
        for rid, lst in recs.items():
            key = lambda r: json.dumps({k: v for k, v in r.items() if not k.startswith('_')}, sort_keys=True)
            cnt = collections.Counter(key(r) for r in lst)
            best = max(lst, key=lambda r: (cnt[key(r)], r['_build']))
            best = dict(best)
            best['_caches'] = len(lst)
            best['_agree'] = cnt[key(best)]
            out[loc][rid] = best
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('caches', nargs='+')
    ap.add_argument('--db', default='world')
    ap.add_argument('--outdir', required=True)
    ap.add_argument('--min-caches', type=int, default=1, help='write SQL only for records seen in at least this many caches')
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    W = a.db

    quests = merge(a.caches, 'quests')
    creatures = merge(a.caches, 'creatures')
    gos = merge(a.caches, 'gameobjects')

    ours = {int(r[0]): r for r in db(W, "SELECT ID, VerifiedBuild, REPLACE(REPLACE(LogTitle,'\\t',' '),'\\n',' '), "
                                         "MD5(COALESCE(QuestDescription,'')), MD5(COALESCE(LogDescription,'')) FROM quest_template")}
    loc_rows = {(int(r[0]), r[1]): int(r[2]) for r in db(W, "SELECT ID, locale, LENGTH(COALESCE(QuestDescription,'')) FROM quest_template_locale")}
    ct = {int(r[0]): r[1] for r in db(W, "SELECT entry, REPLACE(name,'\\t',' ') FROM creature_template")}
    ctm = collections.defaultdict(set)
    for r in db(W, 'SELECT CreatureID, CreatureDisplayID FROM creature_template_model'):
        ctm[int(r[0])].add(int(r[1]))
    gt = {int(r[0]): (int(r[1]), int(r[2]), r[3]) for r in db(W, "SELECT entry, type, displayId, REPLACE(name,'\\t',' ') FROM gameobject_template")}

    import hashlib
    md5 = lambda s: hashlib.md5(s.encode('utf-8')).hexdigest()

    # ---------------- quests
    rows, ph_sql, loc_sql, missing = [], [], [], {}
    en = quests.get('enUS', {})
    for qid, q in sorted(en.items()):
        o = ours.get(qid)
        if o is None:
            status = 'missing'
            missing[qid] = q
        elif o[1] == '-1':
            status = 'placeholder'
        elif md5(q['QuestDescription']) != o[3] or md5(q['LogDescription']) != o[4]:
            status = 'text differs'
        else:
            status = 'same'
        rows.append((qid, q['LogTitle'], status, q['_caches'], q['_agree'], q['_build']))
        if status == 'placeholder' and q['_caches'] >= a.min_caches:
            ph_sql += placeholder_sql(qid, q, lambda typ, oid: (typ != 0 or oid in ct) and (typ != 2 or oid in gt))
    for loc, recs in sorted(quests.items()):
        if loc == 'enUS':
            continue
        for qid, q in sorted(recs.items()):
            if qid not in ours or q['_caches'] < a.min_caches or loc_rows.get((qid, loc), 0) > 0:
                continue
            if (qid, loc) in loc_rows:
                # a row with a title only (from tooltips): fill the empty texts, keep what is there
                fill = ', '.join("`%s`=IF(COALESCE(`%s`,'')='',%s,`%s`)" % (k, k, esc(q[k]), k) for k in (
                    'LogTitle', 'LogDescription', 'QuestDescription', 'AreaDescription', 'PortraitGiverText', 'PortraitGiverName',
                    'PortraitTurnInText', 'PortraitTurnInName', 'QuestCompletionLog') if q[k])
                loc_sql.append('UPDATE `quest_template_locale` SET %s WHERE `ID`=%d AND `locale`=%s;' % (fill, qid, esc(loc)))
                rows.append((qid, q['LogTitle'], loc + ' empty texts filled', q['_caches'], q['_agree'], q['_build']))
                continue
            loc_sql.append('DELETE FROM `quest_template_locale` WHERE `ID`=%d AND `locale`=%s;' % (qid, esc(loc)))
            loc_sql.append('INSERT INTO `quest_template_locale` (`ID`,`locale`,`LogTitle`,`LogDescription`,`QuestDescription`,`AreaDescription`,'
                           '`PortraitGiverText`,`PortraitGiverName`,`PortraitTurnInText`,`PortraitTurnInName`,`QuestCompletionLog`,`VerifiedBuild`) '
                           'VALUES (%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d);' % (
                               qid, esc(loc), esc(q['LogTitle']), esc(q['LogDescription']), esc(q['QuestDescription']), esc(q['AreaDescription']),
                               esc(q['PortraitGiverText']), esc(q['PortraitGiverName']), esc(q['PortraitTurnInText']), esc(q['PortraitTurnInName']),
                               esc(q['QuestCompletionLog']), q['_build']))
            rows.append((qid, q['LogTitle'], loc + ' text added', q['_caches'], q['_agree'], q['_build']))

    with open(os.path.join(a.outdir, 'quests.tsv'), 'w', encoding='utf-8') as f:
        f.write('quest\ttitle\tstatus\tcaches\tagree\tbuild\n')
        for r in rows:
            f.write('\t'.join(map(str, r)) + '\n')
    json.dump({str(k): v for k, v in missing.items()}, open(os.path.join(a.outdir, 'missing_quests.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    head = ['-- From players\' WoW Forever cache files (tools/forever_cache). Safe to run again. Review before use.', '']
    open(os.path.join(a.outdir, 'placeholder_quests.sql'), 'w', encoding='utf-8').write('\n'.join(head + ph_sql) + '\n')
    open(os.path.join(a.outdir, 'quest_locales.sql'), 'w', encoding='utf-8').write('\n'.join(head + loc_sql) + '\n')

    # ---------------- creatures (report only: faction, level and flags are not in the cache)
    with open(os.path.join(a.outdir, 'creatures.tsv'), 'w', encoding='utf-8') as f:
        f.write('entry\tname\ttitle\tstatus\ttype\tfamily\tclassification\tdisplays\tcaches\n')
        for cid, c in sorted(creatures.get('enUS', {}).items()):
            disp = [d['CreatureDisplayID'] for d in c['Displays']]
            if cid not in ct:
                st = 'no template'
            elif ct[cid] != c['Name'][0]:
                st = 'name differs (ours %s)' % ct[cid]
            elif disp and not (set(disp) & ctm[cid]):
                st = 'model differs (ours %s)' % ','.join(map(str, sorted(ctm[cid])))
            else:
                continue
            f.write('\t'.join(map(str, (cid, c['Name'][0], c['Title'], st, c['CreatureType'], c['CreatureFamily'], c['Classification'],
                                        ','.join(map(str, disp)), c['_caches']))) + '\n')

    # ---------------- game objects
    go_sql = list(head)
    with open(os.path.join(a.outdir, 'gameobjects.tsv'), 'w', encoding='utf-8') as f:
        f.write('entry\tname\tstatus\ttype\tdisplay\tcaches\n')
        for gid, g in sorted(gos.get('enUS', {}).items()):
            if gid not in gt:
                st = 'no template'
                if g['_caches'] >= a.min_caches:
                    cols = ','.join('`Data%d`' % i for i in range(35))
                    go_sql.append('DELETE FROM `gameobject_template` WHERE `entry`=%d;' % gid)
                    go_sql.append('INSERT INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,%s,'
                                  '`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES (%d,%d,%d,%s,%s,%s,%s,%s,%s,%d,%d,%d);' % (
                                      cols, gid, g['Type'], g['DisplayID'], esc(g['Name'][0]), esc(g['IconName']), esc(g['CastBarCaption']),
                                      esc(g['UnkString']), repr(g['Size']), ','.join(map(str, g['Data'])), g['ContentTuningId'], g['RequiredLevel'],
                                      g['_build']))
            elif gt[gid][0] != g['Type'] or gt[gid][1] != g['DisplayID']:
                st = 'type/model differs (ours %d/%d)' % gt[gid][:2]
            else:
                continue
            f.write('\t'.join(map(str, (gid, g['Name'][0], st, g['Type'], g['DisplayID'], g['_caches']))) + '\n')
    open(os.path.join(a.outdir, 'gameobjects_missing.sql'), 'w', encoding='utf-8').write('\n'.join(go_sql) + '\n')

    c = collections.Counter(r[2] for r in rows)
    print('quests:', dict(c))
    print('creatures cached:', sum(len(v) for v in creatures.values()), 'objects cached:', sum(len(v) for v in gos.values()))


def placeholder_sql(qid, q, known):
    """Replace a placeholder quest (VerifiedBuild -1) with the cached real data; givers/turn-ins stay as they are.
    The objectives are replaced only when every NPC/object they name exists in our world: Blizzard's often use a credit NPC we lack,
    and then our own working objectives stay."""
    L = ['-- %d %s: real text and rewards from players\' caches (%d cache(s), build %d)' % (qid, q['LogTitle'], q['_caches'], q['_build'])]
    sets = ['`QuestDescription`=%s' % esc(q['QuestDescription']), '`LogDescription`=%s' % esc(q['LogDescription']),
            '`AreaDescription`=%s' % esc(q['AreaDescription']), '`QuestCompletionLog`=%s' % esc(q['QuestCompletionLog']),
            '`LogTitle`=%s' % esc(q['LogTitle']), '`QuestSortID`=%d' % q['QuestSortID'], '`QuestInfoID`=%d' % q['QuestInfoID'],
            '`SuggestedGroupNum`=%d' % q['SuggestedGroupNum'], '`RewardNextQuest`=%d' % q['RewardNextQuest'],
            '`RewardXPDifficulty`=%d' % q['RewardXPDifficulty'], '`RewardXPMultiplier`=%s' % repr(q['RewardXPMultiplier']),
            '`RewardMoneyDifficulty`=%d' % q['RewardMoneyDifficulty'], '`RewardMoneyMultiplier`=%s' % repr(q['RewardMoneyMultiplier']),
            '`RewardSpell`=%d' % q['RewardSpell'], '`StartItem`=%d' % q['StartItem'], '`Flags`=%d' % q['Flags'], '`FlagsEx`=%d' % q['FlagsEx'],
            '`AllowableRaces`=%d' % q['AllowableRaces'], '`TimeAllowed`=%d' % q['TimeAllowed'], '`VerifiedBuild`=%d' % q['_build']]
    for i, it in enumerate(q['RewardItems'], 1):
        sets += ['`RewardItem%d`=%d' % (i, it['ItemID']), '`RewardAmount%d`=%d' % (i, it['Amount']),
                 '`ItemDrop%d`=%d' % (i, it['ItemDrop']), '`ItemDropQuantity%d`=%d' % (i, it['ItemDropQuantity'])]
    for i, it in enumerate(q['RewardChoiceItems'], 1):
        sets += ['`RewardChoiceItemID%d`=%d' % (i, it['ItemID']), '`RewardChoiceItemQuantity%d`=%d' % (i, it['Quantity']),
                 '`RewardChoiceItemDisplayID%d`=%d' % (i, it['DisplayID'])]
    for i, fr in enumerate(q['RewardFactions'], 1):
        sets += ['`RewardFactionID%d`=%d' % (i, fr['FactionID']), '`RewardFactionValue%d`=%d' % (i, fr['Value']),
                 '`RewardFactionOverride%d`=%d' % (i, fr['Override']), '`RewardFactionCapIn%d`=%d' % (i, fr['CapIn'])]
    L.append('UPDATE `quest_template` SET %s WHERE `ID`=%d AND `VerifiedBuild`=-1;' % (', '.join(sets), qid))
    lack = [o['ObjectID'] for o in q['Objectives'] if not known(o['Type'], o['ObjectID'])]
    if lack:
        L.append('-- objectives kept as they are: the real ones need %s, which our world does not have' % ','.join(map(str, lack)))
    else:
        L.append('DELETE FROM `quest_objectives` WHERE `QuestID`=%d;' % qid)
    if q['Objectives'] and not lack:
        L.append('DELETE FROM `quest_objectives` WHERE `ID` IN (%s);' % ','.join(str(o['ID']) for o in q['Objectives']))
        L.append('INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,`Flags2`,'
                 '`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES ' + ','.join(
                     '(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%s,%d)' % (o['ID'], qid, o['Type'], k, o['StorageIndex'], o['ObjectID'], o['Amount'],
                                                                         o['ConditionalAmount'], o['Flags'], o['Flags2'] & ~1, repr(o['ProgressBarWeight']),
                                                                         o['ParentObjectiveID'], o['Visible'], esc(o['Description']) if o['Description'] else 'NULL',
                                                                         q['_build']) for k, o in enumerate(q['Objectives'])) + ';')
    L.append('DELETE FROM `quest_classic_money` WHERE `ID`=%d;' % qid)
    if q['RewardMoney']:
        L.append('INSERT INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (%d,%d,%d);' % (qid, q['RewardMoney'], q['RewardBonusMoney']))
    L.append('UPDATE `quest_classic_level` SET `QuestLevel`=%d, `MinLevel`=%d WHERE `ID`=%d;' % (q['QuestLevel'], q['MinLevel'], qid))
    L.append('UPDATE `quest_template_classic_level` SET `LevelType`=%d, `QuestLevel`=%d WHERE `ID`=%d;' % (q['LevelType'], q['QuestLevel'], qid))
    # The cache has no reward text (that comes with the gossip answer), so the placeholder reward text (VerifiedBuild -1) stays.
    L.append('')
    return L


if __name__ == '__main__':
    main()
