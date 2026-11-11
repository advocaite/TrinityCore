#!/usr/bin/env python3
"""Write SQL drafts from eg_compare.py's report and the EG Link 70862 files.

    REPO=. python3 -I tools/eg_link/eg_sql.py <eg build folder> <compare.json> <out folder>
"""
import collections
import json
import os
import re
import subprocess
import sys

EG, REP, OUT = sys.argv[1:4]
rep = json.load(open(REP, encoding='utf-8'))
REPO = os.environ.get('REPO', '.')
ITEMS = os.environ.get('CLIENT_ITEM_IDS', 'client_item_ids.json')


def load(name, key):
    return json.load(open(os.path.join(EG, name), encoding='utf-8'))[key]


def db(sql):
    o = subprocess.run(['mysql', '-N', '-B', '--default-character-set=utf8mb4', 'world', '-e', sql],
                       capture_output=True, text=True, check=True).stdout
    return [l.split('\t') for l in o.splitlines()]


def esc(s):
    return "'" + str(s).replace('\\', '\\\\').replace("'", "''").replace('\n', '\\n').replace('\r', '') + "'"


def txt(s):
    """EG keeps the game's $B line breaks; our rows use real new lines."""
    return (s or '').replace('$B', '\n').replace('$b', '\n')


CREDIT = ['-- Data from Efficient Games (https://efficient.games), licensed under CC BY 4.0',
          '-- (https://creativecommons.org/licenses/by/4.0/). Source: github.com/JIVESCORP/eg-link-output-wowf, build 1.60.1.70862,',
          '-- collected 2026-11-07..10 from 4 players on the live realm. Changed: picked and converted to our tables; only rows we lacked',
          '-- or had different from live. Where our own live captures (2026_11_10_60/61) cover a record, ours is kept.']
BUILD = 70862

Q = load('quests.enUS.json', 'quests')
OD = load('object_details.enUS.json', 'objects')
V = load('vendors.json', 'vendors')
T = load('trainers.json', 'trainers')

ct = {int(r[0]) for r in db('SELECT entry FROM creature_template')}
gt = {int(r[0]) for r in db('SELECT entry FROM gameobject_template')}
used_obj = {int(r[0]): int(r[1]) for r in db('SELECT ID, QuestID FROM quest_objectives')}
ourobj = collections.defaultdict(list)
for r in db('SELECT ID, QuestID, Type, ObjectID, Amount FROM quest_objectives'):
    ourobj[int(r[1])].append(tuple(map(int, r)))
own = open(REPO + '/sql/custom/world/2026_11_10_60_world_forever_live_sniffs_1107_1110.sql').read() + \
    open(REPO + '/sql/custom/world/2026_11_10_61_world_forever_live_sniff_1110b.sql').read()
in_own = lambda i: re.search(r'\b%d\b' % i, own) is not None

W = []   # main fill file
notes = collections.Counter()

# ---------------------------------------------------------------- 1. object templates we lack
W += ['', '-- 1. Object templates our world lacks (template and quest loot list only, no spawns). Fill-only: rows we have stay.']
rows, qi = [], []
for r in rep['objects']:
    if r['status'] != 'no template' or in_own(r['id']):
        continue
    g = OD[str(r['id'])]
    if not g.get('name'):
        notes['object skipped: no name'] += 1
        continue
    data = (g.get('data') or [0] * 35)[:35]
    data += [0] * (35 - len(data))
    rows.append('(%d,%d,%d,%s,%s,%s,%s,%s,%s,0,0,%d)' % (r['id'], g['type'], g['displayId'], esc(g['name']), esc(''), esc(''), esc(''),
                                                      repr(float(g.get('size', 1.0))), ','.join(map(str, data)), BUILD))
    for k, it in enumerate(g.get('questItems', [])):
        qi.append('(%d,%d,%d,%d)' % (r['id'], k, it, BUILD))
cols = ','.join('`Data%d`' % i for i in range(35))
for i in range(0, len(rows), 200):
    W.append('INSERT IGNORE INTO `gameobject_template` (`entry`,`type`,`displayId`,`name`,`IconName`,`castBarCaption`,`unk1`,`size`,%s,'
             '`ContentTuningId`,`RequiredLevel`,`VerifiedBuild`) VALUES\n%s;' % (cols, ',\n'.join(rows[i:i + 200])))
if qi:
    W.append('INSERT IGNORE INTO `gameobject_questitem` (`GameObjectEntry`,`Idx`,`ItemId`,`VerifiedBuild`) VALUES\n%s;' % ',\n'.join(qi))
notes['object templates'] = len(rows)
notes['object quest items'] = len(qi)

# ---------------------------------------------------------------- 2. quest giver / turn-in links we lack
W += ['', '-- 2. Quest givers and turn-ins players used on live that our quests lack (the quests and NPCs/objects exist).']
for r in rep['quests']:
    for kind, who in (('starter', r.get('lack_givers', [])), ('ender', r.get('lack_enders', []))):
        for k, i in who:
            tbl = ('creature_quest%s' if k == 'npc' else 'gameobject_quest%s') % kind
            W.append('-- %d %s' % (r['id'], r['title']))
            W.append('INSERT IGNORE INTO `%s` (`id`,`quest`,`VerifiedBuild`) VALUES (%d,%d,%d);' % (tbl, i, r['id'], BUILD))
            if k == 'npc':
                W.append('UPDATE `creature_template` SET `npcflag`=`npcflag`|2 WHERE `entry`=%d;' % i)
            notes['quest links'] += 1

# ---------------------------------------------------------------- 3. placeholder quests: the live record
W += ['', '-- 3. Quests we built with placeholder text (VerifiedBuild -1): the live text, levels, rewards and objectives.',
      '--    Givers and turn-ins stay ours. Reward text is not in the data, so the placeholder reward text stays.']
TYPES = {'monster': 0, 'item': 1, 'gameObject': 2, 'talkTo': 3}
for r in rep['quests']:
    if r['status'] != 'placeholder':
        continue
    q = Q[str(r['id'])]
    if not q.get('description'):
        W.append('-- %d %s: live record has no text yet, left as it is' % (r['id'], r['title']))
        continue
    rw = q.get('rewards', {})
    sets = ['`LogTitle`=%s' % esc(q['title']), '`QuestDescription`=%s' % esc(txt(q.get('description'))),
            '`LogDescription`=%s' % esc(txt(q.get('objectivesText'))), '`AreaDescription`=%s' % esc(txt(q.get('areaDescription'))),
            '`QuestCompletionLog`=%s' % esc(txt(q.get('completionText'))), '`Flags`=%d' % q.get('flags', 0),
            '`FlagsEx`=%d' % q.get('flagsEx', 0), '`AllowableRaces`=%d' % int(q.get('raceMask', '0'), 16),
            '`RewardXPDifficulty`=%d' % rw.get('xpDifficulty', 0), '`RewardXPMultiplier`=%s' % repr(float(rw.get('xpMultiplier', 1.0))),
            '`RewardNextQuest`=%d' % q.get('nextQuestId', 0), '`StartItem`=%d' % q.get('startItemId', 0),
            '`RewardSpell`=%d' % rw.get('spellId', 0), '`TimeAllowed`=%d' % q.get('timeLimitSeconds', 0), '`VerifiedBuild`=%d' % BUILD]
    if q.get('zoneAreaId'):
        sets.append('`QuestSortID`=%d' % q['zoneAreaId'])
    elif q.get('categoryId'):
        sets.append('`QuestSortID`=%d' % -q['categoryId'])
    if q.get('questInfoId') is not None:
        sets.append('`QuestInfoID`=%d' % q.get('questInfoId', 0))
    items = rw.get('items', [])[:4]
    for i in range(4):
        it = items[i] if i < len(items) else {'itemId': 0, 'count': 0}
        sets += ['`RewardItem%d`=%d' % (i + 1, it['itemId']), '`RewardAmount%d`=%d' % (i + 1, it['count'])]
    ch = rw.get('choices', [])[:6]
    for i in range(6):
        it = ch[i] if i < len(ch) else {'itemId': 0, 'count': 0}
        sets += ['`RewardChoiceItemID%d`=%d' % (i + 1, it['itemId']), '`RewardChoiceItemQuantity%d`=%d' % (i + 1, it['count'])]
    rp = rw.get('reputation', [])[:5]
    for i in range(5):
        f = rp[i] if i < len(rp) else {'factionId': 0, 'tier': 0}
        sets += ['`RewardFactionID%d`=%d' % (i + 1, f['factionId']), '`RewardFactionValue%d`=%d' % (i + 1, f.get('tier', 0)),
                 '`RewardFactionOverride%d`=%d' % (i + 1, f.get('override', 0))]
    W.append('-- %d %s (live record, %d player(s))' % (r['id'], q['title'], q['sources']))
    W.append('UPDATE `quest_template` SET %s WHERE `ID`=%d AND `VerifiedBuild`=-1;' % (', '.join(sets), r['id']))
    W.append('DELETE FROM `quest_classic_money` WHERE `ID`=%d AND EXISTS (SELECT 1 FROM `quest_template` WHERE `ID`=%d AND `VerifiedBuild`=%d);'
             % (r['id'], r['id'], BUILD))
    if rw.get('money'):
        W.append('INSERT IGNORE INTO `quest_classic_money` (`ID`,`RewardMoney`,`RewardMoneyMaxLevel`) VALUES (%d,%d,0);' % (r['id'], rw['money']))
    W.append('UPDATE `quest_classic_level` SET `QuestLevel`=%d, `MinLevel`=%d WHERE `ID`=%d;' % (q.get('level', 0), q.get('minLevel', 0), r['id']))
    W.append('UPDATE `quest_template_classic_level` SET `LevelType`=2, `QuestLevel`=%d WHERE `ID`=%d;' % (q.get('level', 0), r['id']))
    obs = q.get('objectives', [])
    ok = obs and all(o.get('type') in TYPES and (o['type'] != 'monster' or o['objectId'] in ct) and (o['type'] != 'gameObject' or o['objectId'] in gt)
                     and used_obj.get(o['id'], r['id']) == r['id'] for o in obs)
    if ok:
        W.append('DELETE FROM `quest_objectives` WHERE `QuestID`=%d;' % r['id'])
        W.append('INSERT INTO `quest_objectives` (`ID`,`QuestID`,`Type`,`Order`,`StorageIndex`,`ObjectID`,`Amount`,`ConditionalAmount`,`Flags`,'
                 '`Flags2`,`ProgressBarWeight`,`ParentObjectiveID`,`Visible`,`Description`,`VerifiedBuild`) VALUES %s;' % ','.join(
                     '(%d,%d,%d,%d,%d,%d,%d,0,0,0,0,0,1,%s,%d)' % (o['id'], r['id'], TYPES[o['type']], k, o.get('storageIndex', k), o['objectId'],
                                                                o.get('amount', 1), esc(o['text']) if o.get('text') else 'NULL', BUILD)
                     for k, o in enumerate(obs)))
    elif obs:
        W.append('-- objectives kept as ours: the live ones name something our world lacks')
    notes['placeholder quests'] += 1

# ---------------------------------------------------------------- 4. objective amounts live changed
W += ['', '-- 4. Quest objectives whose amount is different on live (same target).']
for qid, q in sorted(Q.items(), key=lambda kv: int(kv[0])):
    qid = int(qid)
    for o in q.get('objectives', []):
        m = [x for x in ourobj.get(qid, []) if x[3] == o.get('objectId') and x[2] == o.get('typeId')]
        if m and m[0][4] != o.get('amount') and o.get('amount'):
            W.append('UPDATE `quest_objectives` SET `Amount`=%d WHERE `ID`=%d AND `QuestID`=%d AND `ObjectID`=%d; -- %d %s: live %d, was %d'
                     % (o['amount'], m[0][0], qid, o['objectId'], qid, q['title'], o['amount'], m[0][4]))
            notes['objective amounts'] += 1

# ---------------------------------------------------------------- 5. quest text live rewrote
W += ['', '-- 5. Quests whose story or objective line is different on live (not only a placeholder like $1oa or a line break).']
ourtext = {int(r[0]): [bytes.fromhex(x).decode('utf-8', 'replace') for x in r[1:]] for r in db(
    "SELECT ID, HEX(COALESCE(QuestDescription,'')), HEX(COALESCE(LogDescription,'')) FROM quest_template")}
norm = lambda s: re.sub(r'\s+', ' ', (s or '').replace('$B', '\n').replace('$b', '\n')).strip().lower().replace('$n', '$n')
pl = re.compile(r'\$\d+o[a-z]')
for r in rep['quests']:
    if r['status'] != 'text differs' or r['junk']:
        continue
    q = Q[str(r['id'])]
    od, ol = ourtext[r['id']][:2]
    sets = []
    if q.get('description') and norm(q['description']).replace('$c', '$C'.lower()) != norm(od):
        a, b = norm(q['description']), norm(od)
        if re.sub(r'\$[a-z]', '', a) != re.sub(r'\$[a-z]', '', b):
            sets.append('`QuestDescription`=%s' % esc(txt(q['description'])))
    if q.get('objectivesText') and norm(q['objectivesText']) != norm(ol) and not pl.search(q['objectivesText']):
        if re.sub(r'\$[a-z]', '', norm(q['objectivesText'])) != re.sub(r'\$[a-z]', '', norm(ol)):
            sets.append('`LogDescription`=%s' % esc(txt(q['objectivesText'])))
    if sets and not in_own(r['id']):
        W.append('-- %d %s' % (r['id'], q['title']))
        W.append('UPDATE `quest_template` SET %s WHERE `ID`=%d;' % (', '.join(sets), r['id']))
        notes['quest texts'] += 1

# ---------------------------------------------------------------- 6. vendor items live sells and we don't
W += ['', '-- 6. Items live vendors sell that ours lack (vendor exists here; items bought with other items/currencies are left out).',
      '--    Limited stock: the most players saw, restock time 2 h like our other limited rows (ESTIMATED).']
slots = {int(r[0]): int(r[1]) for r in db('SELECT entry, MAX(slot) FROM npc_vendor GROUP BY entry')}
for r in rep['vendors']:
    if not r['template'] or not r['add'] or r['in_own_sniff']:
        continue
    adds = [a for a in r['add'] if a['type'] == 1 and a['known'] and not a['costItems']]
    if not adds:
        continue
    W.append('-- %d %s (%d visit(s) on live)' % (r['id'], r['name'], r['visits'] or 0))
    if not r['vendor_flag']:
        W.append('UPDATE `creature_template` SET `npcflag`=`npcflag`|128 WHERE `entry`=%d;' % r['id'])
    s = slots.get(r['id'], -1)
    vals = []
    for a in sorted(adds, key=lambda a: a['item']):
        s += 1
        vals.append('(%d,%d,%d,%d,%d,0,1,%d) /* %s */' % (r['id'], s, a['item'], a['limited'] or 0, 7200 if a['limited'] else 0, BUILD, a['name'].replace('*/', '')))
        notes['vendor items'] += 1
    W.append('INSERT IGNORE INTO `npc_vendor` (`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`type`,`VerifiedBuild`) VALUES %s;'
             % ','.join(vals))

# ---------------------------------------------------------------- 7. trainer levels live changed
W += ['', '-- 7. Trainer spells live teaches at a different level (apprentice professions at 1, druid Primal Bite).']
tl = collections.defaultdict(set)
for r in rep['trainers']:
    if not r['trainer_id'] or r['in_own_sniff']:
        continue
    for l in r['level']:
        if l['live'] > 0:
            tl[(r['trainer_id'], l['spell'], l['name'], l['ours'])].add(l['live'])
for (tid, sid, name, ours), lv in sorted(tl.items()):
    if len(lv) != 1:
        continue
    W.append('UPDATE `trainer_spell` SET `ReqLevel`=%d WHERE `TrainerId`=%d AND `SpellId`=%d; -- %s, was %d' % (min(lv), tid, sid, name, ours))
    notes['trainer levels'] += 1

# ---------------------------------------------------------------- trainer prices (own file)
P = []
tp = collections.defaultdict(set)
for r in rep['trainers']:
    if not r['trainer_id'] or r['in_own_sniff']:
        continue
    for p in r['price']:
        tp[(r['trainer_id'], p['spell'], p['name'], p['rank'] or '', p['ours'])].add(p['live'])
by_t = collections.defaultdict(list)
skipped = 0
for (tid, sid, name, rank, ours), lv in sorted(tp.items()):
    if len(lv) != 1:
        skipped += 1
        continue
    by_t[tid].append('UPDATE `trainer_spell` SET `MoneyCost`=%d WHERE `TrainerId`=%d AND `SpellId`=%d; -- %s %s, was %d'
                     % (lv.pop(), tid, sid, name, rank, ours))
tnames = collections.defaultdict(set)
for r in rep['trainers']:
    if r['trainer_id']:
        tnames[r['trainer_id']].add(r['name'])
for tid, lines in sorted(by_t.items()):
    P.append('')
    P.append('-- trainer %d (%s): %d spells' % (tid, ', '.join(sorted(tnames[tid])), len(lines)))
    P += lines
notes['trainer prices'] = sum(len(v) for v in by_t.values())
notes['trainer prices skipped (players disagree)'] = skipped

os.makedirs(OUT, exist_ok=True)
head = ['-- WoW Forever: world rows from live players\' EG Link data (build 70862) that our world lacked or had different.'] + CREDIT + \
       ['-- Safe to run again.']
open(os.path.join(OUT, '2026_11_11_10_world_forever_eg_link_live_data.sql'), 'w', encoding='utf-8').write('\n'.join(head + W) + '\n')
ph = ['-- WoW Forever: trainer prices as live trainers show them (build 70862). Live sells at the neutral price up to Friendly and',
      '-- 10% off at Honored, so the price here is the Friendly one (or Honored / 0.9). Many class spells cost less on live than the',
      '-- vanilla prices we had; most profession recipes cost a little more (ours were 10% under vanilla).'] + CREDIT + ['-- Safe to run again.']
open(os.path.join(OUT, '2026_11_11_11_world_forever_eg_link_trainer_prices.sql'), 'w', encoding='utf-8').write('\n'.join(ph + P) + '\n')
for k, v in notes.items():
    print(k, v)
