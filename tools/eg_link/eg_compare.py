#!/usr/bin/env python3
"""Compare Efficient Games' EG Link output (CC BY 4.0) for one build with our world DB (read only).

    REPO=. CLIENT_ITEM_IDS=client_item_ids.json python3 -I tools/eg_link/eg_compare.py <eg build folder> <out folder>

client_item_ids.json: a JSON list of the Item.db2 ids of the client build. The world DB is read with the mysql client (read only).

Writes JSON/TSV reports into <out>. SQL is written by eg_sql.py from these reports.
"""
import collections
import hashlib
import json
import os
import subprocess
import sys

EG, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
REPO = os.environ.get('REPO', '.')
ITEMS = os.environ.get('CLIENT_ITEM_IDS', 'client_item_ids.json')


def db(sql):
    o = subprocess.run(['mysql', '-N', '--default-character-set=utf8mb4', 'world', '-e', sql],
                       capture_output=True, text=True, check=True).stdout
    return [l.split('\t') for l in o.splitlines()]


def load(name, key):
    return json.load(open(os.path.join(EG, name), encoding='utf-8'))[key]


md5 = lambda s: hashlib.md5((s or '').encode('utf-8')).hexdigest()

# ---------------- our world
ct = {int(r[0]): (r[1], int(r[2]), int(r[3])) for r in db("SELECT entry, REPLACE(name,'\\t',' '), npcflag, faction FROM creature_template")}
ctm = collections.defaultdict(set)
for r in db('SELECT CreatureID, CreatureDisplayID FROM creature_template_model'):
    ctm[int(r[0])].add(int(r[1]))
spawned = collections.Counter(int(r[0]) for r in db('SELECT id FROM creature'))
gt = {int(r[0]): (int(r[1]), int(r[2]), r[3]) for r in db("SELECT entry, type, displayId, REPLACE(name,'\\t',' ') FROM gameobject_template")}
gspawned = collections.Counter(int(r[0]) for r in db('SELECT id FROM gameobject'))
oq = {int(r[0]): r for r in db("SELECT ID, VerifiedBuild, MD5(COALESCE(QuestDescription,'')), MD5(COALESCE(LogDescription,'')), "
                                "REPLACE(LogTitle,'\\t',' '), RewardXPDifficulty, RewardNextQuest, StartItem, Flags FROM quest_template")}
qs = collections.defaultdict(set)
qe = collections.defaultdict(set)
for r in db('SELECT id, quest FROM creature_queststarter'):
    qs[int(r[1])].add(('npc', int(r[0])))
for r in db('SELECT id, quest FROM gameobject_queststarter'):
    qs[int(r[1])].add(('obj', int(r[0])))
for r in db('SELECT id, quest FROM creature_questender'):
    qe[int(r[1])].add(('npc', int(r[0])))
for r in db('SELECT id, quest FROM gameobject_questender'):
    qe[int(r[1])].add(('obj', int(r[0])))
vend = collections.defaultdict(dict)
for r in db('SELECT entry, item, maxcount, ExtendedCost, type FROM npc_vendor'):
    vend[int(r[0])][(int(r[4]), int(r[1]))] = (int(r[2]), int(r[3]))
# vendors can share lists through creature_template.VendorTemplate? (TC has none: npc_vendor is per entry)
trainer_of = {int(r[0]): int(r[1]) for r in db('SELECT CreatureID, TrainerID FROM creature_trainer')}
tspells = collections.defaultdict(dict)
for r in db('SELECT TrainerId, SpellId, MoneyCost, ReqSkillRank, ReqLevel FROM trainer_spell'):
    tspells[int(r[0])][int(r[1])] = (int(r[2]), int(r[3]), int(r[4]))
loot_id = {int(r[0]): int(r[1]) for r in db('SELECT Entry, LootID FROM creature_template_difficulty WHERE DifficultyID=0')}
cloot = collections.defaultdict(dict)
refl = collections.defaultdict(dict)
for r in db('SELECT Entry, Item, Chance, QuestRequired, ItemType FROM reference_loot_template'):
    refl[int(r[0])][int(r[1])] = (float(r[2]), int(r[3]), int(r[4]))
for r in db('SELECT Entry, Item, Chance, QuestRequired, ItemType FROM creature_loot_template'):
    if int(r[4]) == 1:
        for it, v in refl.get(int(r[1]), {}).items():
            if v[2] == 1:
                for it2, v2 in refl.get(it, {}).items():
                    cloot[int(r[0])][it2] = v2
            else:
                cloot[int(r[0])][it] = v
    else:
        cloot[int(r[0])][int(r[1])] = (float(r[2]), int(r[3]), int(r[4]))
objids = {int(r[0]) for r in db('SELECT ID FROM quest_objectives')}
client_items = set(json.load(open(ITEMS)))
eg_items = load('item_hotfixes.enUS.json', 'items')
valid_items = client_items | {int(k) for k, v in eg_items.items() if v['status'] == 'valid'}
withheld_items = {int(k) for k, v in eg_items.items() if v['status'] != 'valid'}
# rows our own live sniffs (11-07/11-10) wrote: prefer those over EG
own_sniff = open(REPO + '/sql/custom/world/2026_11_10_60_world_forever_live_sniffs_1107_1110.sql').read() + \
    open(REPO + '/sql/custom/world/2026_11_10_61_world_forever_live_sniff_1110b.sql').read()

rep = {}

# ---------------- quests
Q = load('quests.enUS.json', 'quests')
npcs = load('npcs.json', 'npcs')
nobjs = load('npcs.json', 'objects')
givers, enders = collections.defaultdict(set), collections.defaultdict(set)
for kind, src in (('npc', npcs), ('obj', nobjs)):
    for i, n in src.items():
        for q in n.get('offers', {}):
            givers[int(q)].add((kind, int(i)))
        for q in n.get('takes', {}):
            enders[int(q)].add((kind, int(i)))
prereq = json.load(open(os.path.join(EG, 'quest_prerequisites.json'), encoding='utf-8'))
turn = load('turn_ins.json', 'quests')


def exists(kind, i):
    return i in ct if kind == 'npc' else i in gt


def placed(kind, i):
    return spawned[i] if kind == 'npc' else gspawned[i]


JUNK = ('[never used]', '[dnt]', 'unused', 'zzold', '(temp disabled)', 'test', 'deprecated', '[ph]', 'zz')


def is_junk(t):
    tl = t.lower()
    return any(tl.startswith(j) or j in tl for j in JUNK if j not in ('test', 'zz')) or tl.startswith('zz') or ' test' in tl or tl.endswith('test')


qrows = []
missing = {}
for qid, q in sorted(Q.items(), key=lambda kv: int(kv[0])):
    qid = int(qid)
    o = oq.get(qid)
    title = q['title']
    if o is None:
        st = 'missing'
    elif o[1] == '-1':
        st = 'placeholder'
    elif md5(q.get('description')) != o[2] or md5(q.get('objectivesText')) != o[3]:
        st = 'text differs'
    else:
        st = 'same'
    g, e = sorted(givers.get(qid, ())), sorted(enders.get(qid, ()))
    info = dict(id=qid, title=title, status=st, sources=q['sources'], level=q.get('level'), junk=is_junk(title),
                givers=g, enders=e)
    if st in ('missing', 'placeholder'):
        need = []
        if not g:
            need.append('giver unknown')
        for k, i in g:
            if not exists(k, i):
                need.append('giver %s %d no template' % (k, i))
            elif not placed(k, i):
                need.append('giver %s %d not spawned' % (k, i))
        if not e:
            need.append('turn-in unknown')
        for k, i in e:
            if not exists(k, i):
                need.append('turn-in %s %d no template' % (k, i))
            elif not placed(k, i):
                need.append('turn-in %s %d not spawned' % (k, i))
        for ob in q.get('objectives', []):
            t, oid = ob.get('type'), ob.get('objectId')
            if t == 'monster' and oid not in ct:
                need.append('target npc %d no template' % oid)
            elif t == 'monster' and not spawned[oid]:
                need.append('target npc %d not spawned' % oid)
            elif t == 'gameObject' and oid not in gt:
                need.append('target object %d no template' % oid)
            elif t == 'item' and oid not in valid_items:
                need.append('item %d %s' % (oid, 'withheld by live server' if oid in withheld_items else 'unknown to client'))
            elif t not in ('monster', 'item', 'gameObject', 'talkTo'):
                need.append('objective type %s needs a script' % t)
            if ob['id'] in objids and st == 'missing':
                need.append('objective id %d already used' % ob['id'])
        for it in q.get('rewards', {}).get('items', []) + q.get('rewards', {}).get('choices', []):
            if it['itemId'] not in valid_items:
                need.append('reward item %d %s' % (it['itemId'], 'withheld' if it['itemId'] in withheld_items else 'unknown'))
        si = q.get('startItemId')
        if si and si not in valid_items:
            need.append('start item %d unknown' % si)
        info['needs'] = need
        info['buildable'] = not need and not info['junk']
        missing[qid] = info
    else:
        # links we lack on quests we have
        lack_g = [x for x in g if x not in qs.get(qid, set()) and exists(*x)]
        lack_e = [x for x in e if x not in qe.get(qid, set()) and exists(*x)]
        if lack_g or lack_e:
            info['lack_givers'], info['lack_enders'] = lack_g, lack_e
    qrows.append(info)
rep['quests'] = qrows

# ---------------- creature templates
CD = load('creature_details.enUS.json', 'creatures')
crows = []
for cid, c in sorted(CD.items(), key=lambda kv: int(kv[0])):
    cid = int(cid)
    disp = [d['displayId'] for d in c.get('displays', [])]
    if cid not in ct:
        st = 'no template'
    elif c.get('name') and ct[cid][0] != c['name']:
        st = 'name differs'
    elif disp and not (set(disp) & ctm[cid]):
        st = 'model differs'
    else:
        continue
    crows.append(dict(id=cid, name=c.get('name', ''), title=c.get('title', ''), status=st, ours=(ct.get(cid, ('',))[0], sorted(ctm[cid])),
                      displays=disp, type=c.get('type'), family=c.get('family'), classification=c.get('classification'),
                      sources=c['sources'], in_own_sniff=str(cid) in own_sniff))
rep['creatures'] = crows

# ---------------- object templates
OD = load('object_details.enUS.json', 'objects')
grows = []
for gid, g in sorted(OD.items(), key=lambda kv: int(kv[0])):
    gid = int(gid)
    if gid not in gt:
        st = 'no template'
    elif gt[gid][0] != g['type'] or gt[gid][1] != g['displayId']:
        st = 'type/model differs'
    elif g.get('name') and gt[gid][2] != g['name']:
        st = 'name differs'
    else:
        continue
    grows.append(dict(id=gid, name=g.get('name', ''), status=st, type=g['type'], display=g['displayId'], ours=gt.get(gid), sources=g['sources'],
                      in_own_sniff=str(gid) in own_sniff))
rep['objects'] = grows

# ---------------- vendors
V = load('vendors.json', 'vendors')
vrows = []
for nid, v in sorted(V.items(), key=lambda kv: int(kv[0])):
    nid = int(nid)
    if 'entries' not in v:
        continue
    live = {}
    for key, e in v['entries'].items():
        if 'itemId' in e:
            live[(1, e['itemId'])] = e
        elif 'currencyId' in e:
            live[(2, e['currencyId'])] = e
    ours = vend.get(nid, {})
    add = [dict(item=k[1], type=k[0], name=e['name'], limited=e.get('stockMax', 0), costItems=e.get('costItems', []),
                known=(k[1] in valid_items) if k[0] == 1 else True) for k, e in live.items() if k not in ours]
    extra = [k[1] for k in ours if k not in live]
    limit = [dict(item=k[1], ours=ours[k][0], live=e.get('stockMax', 0)) for k, e in live.items()
             if k in ours and ours[k][0] != e.get('stockMax', 0)]
    if add or extra or limit or nid not in ct:
        vrows.append(dict(id=nid, name=v['name'], template=nid in ct, vendor_flag=bool(ct.get(nid, ('', 0))[1] & 128), sources=v['sources'],
                          live_items=len(live), our_items=len(ours), add=add, extra=extra, limit=limit, in_own_sniff=str(nid) in own_sniff,
                          visits=v.get('visits')))
rep['vendors'] = vrows

# ---------------- trainers
T = load('trainers.json', 'trainers')
trows = []
for nid, t in sorted(T.items(), key=lambda kv: int(kv[0])):
    nid = int(nid)
    tid = trainer_of.get(nid)
    ours = tspells.get(tid, {}) if tid else {}
    add, price, lvl = [], [], []
    for sid, s in t.get('services', {}).items():
        sid = int(s.get('spellId', sid))
        costs = s.get('costs', {})
        base = costs.get('4') or costs.get('5')
        if base is None and costs.get('6'):
            base = round(costs['6'] / 0.9)
        if sid not in ours:
            add.append(dict(spell=sid, name=s.get('name'), rank=s.get('rankText'), level=s.get('level'), skill=s.get('skillRank'), cost=base))
            continue
        oc, osk, ol = ours[sid]
        if base is not None and base != oc:
            price.append(dict(spell=sid, name=s.get('name'), rank=s.get('rankText'), ours=oc, live=base, costs=costs))
        if s.get('level') is not None and s['level'] != ol and not (s['level'] == 0 and ol <= 1):
            lvl.append(dict(spell=sid, name=s.get('name'), ours=ol, live=s['level']))
    extra = [sid for sid in ours if str(sid) not in t.get('services', {}) and sid not in {int(x.get('spellId', 0)) for x in t.get('services', {}).values()}]
    trows.append(dict(id=nid, name=t['name'], trainer_id=tid, template=nid in ct, live_spells=len(t.get('services', {})), our_spells=len(ours),
                      add=add, price=price, level=lvl, extra=extra, in_own_sniff=str(nid) in own_sniff, sources=t['sources']))
rep['trainers'] = trows

# ---------------- loot (creatures only)
L = json.load(open(os.path.join(EG, 'loot.json'), encoding='utf-8'))
kills = L.get('kills', {})
lrows = []
for key, src in L['sources'].items():
    if not key.startswith('npc:') or '|' in key:
        continue
    cid = int(key[4:])
    k = kills.get(key, {})
    deaths = k.get('deaths', 0) or src.get('loots', 0)
    lid = loot_id.get(cid, 0)
    ours = cloot.get(lid, {}) if lid else {}
    new = []
    for it, d in src.get('items', {}).items():
        it = int(it)
        if it not in ours:
            new.append(dict(item=it, chance=d.get('chance'), drops=d.get('drops'), known=it in valid_items))
    if new:
        lrows.append(dict(id=cid, name=ct.get(cid, ('?',))[0], template=cid in ct, lootid=lid, our_rows=len(ours), kills=deaths,
                          loots=src.get('loots'), new=new, in_own_sniff=str(cid) in own_sniff))
rep['loot'] = lrows

json.dump(rep, open(os.path.join(OUT, 'compare.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
c = collections.Counter(r['status'] for r in qrows)
print('quests', dict(c), 'buildable', sum(1 for m in missing.values() if m['buildable']),
      'missing not junk', sum(1 for m in missing.values() if not m['junk']))
print('quests we have but lack giver/ender links', sum(1 for r in qrows if 'lack_givers' in r))
print('creatures', collections.Counter(r['status'] for r in crows))
print('objects', collections.Counter(r['status'] for r in grows))
print('vendors with diffs', len(vrows), 'items to add', sum(len(r['add']) for r in vrows))
print('trainers', len(trows), 'spells to add', sum(len(r['add']) for r in trows), 'price diffs', sum(len(r['price']) for r in trows),
      'level diffs', sum(len(r['level']) for r in trows))
print('loot creatures with new items', len(lrows), sum(len(r['new']) for r in lrows))
