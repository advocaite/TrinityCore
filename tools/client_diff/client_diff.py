#!/usr/bin/env python3
"""What a new client build changed, and how our server data compares with it.

Run it when a client build lands (or after a hotfix/world import):

    python3 -I tools/client_diff/client_diff.py --repo . \
        --old <old build dbc dir> --new <new build dbc dir> \
        --hotfixes hotfixes.sql.gz --world world.sql.gz \
        [--pending-sql dir-or-patch ...] --out <out dir>

--old / --new: folders of .db2 files (server/data/dbc/enUS after mapextractor, or any copy). The old folder may hold only
some tables; a table missing on either side is listed as "not compared". --hotfixes / --world: mysqldump files of our
databases (the PC backup folders hold them). --pending-sql: SQL files or patches not yet in the dump (the next build's
queue), so rows they already fix are marked. Writes report.json, report.html and one TSV per finding list to --out.
"""
import argparse
import collections
import datetime
import glob
import html
import json
import os
import re
import sqlite3
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wdc5  # noqa: E402
import sqldump  # noqa: E402

CLASSES = {1: 'Warrior', 2: 'Paladin', 3: 'Hunter', 4: 'Rogue', 5: 'Priest', 7: 'Shaman', 8: 'Mage', 9: 'Warlock',
           11: 'Druid'}
SPELL_FAMILY = {3: 'Mage', 4: 'Warrior', 5: 'Warlock', 6: 'Priest', 7: 'Druid', 8: 'Rogue', 9: 'Hunter', 10: 'Paladin',
                11: 'Shaman'}
QUALITY = ['Poor', 'Common', 'Uncommon', 'Rare', 'Epic', 'Legendary', 'Artifact', 'Heirloom']
STAT = {0: 'Mana', 1: 'Health', 3: 'Agility', 4: 'Strength', 5: 'Intellect', 6: 'Spirit', 7: 'Stamina', 12: 'Defense',
        13: 'Dodge', 14: 'Parry', 15: 'Block', 16: 'Melee hit', 17: 'Ranged hit', 18: 'Spell hit', 19: 'Melee crit',
        20: 'Ranged crit', 21: 'Spell crit', 31: 'Hit', 32: 'Crit', 35: 'Resilience', 36: 'Haste', 37: 'Expertise',
        38: 'Attack power', 39: 'Ranged attack power', 41: 'Healing', 42: 'Spell damage', 43: 'Mana per 5',
        45: 'Spell power', 46: 'Health per 5', 47: 'Spell penetration', 48: 'Block value', 50: 'Armor',
        51: 'Fire resistance', 52: 'Frost resistance', 53: 'Holy resistance', 54: 'Shadow resistance',
        55: 'Nature resistance', 56: 'Arcane resistance',
        # 1.60 school damage stats, named from the items that carry them (Tome of Fiery Arcana = 85, ...)
        85: 'Fire spell damage', 87: 'Frost spell damage', 88: 'Shadow spell damage', 89: 'Arcane spell damage'}
INVTYPE_SLOT = {1: 0, 4: 0, 5: 0, 7: 0, 15: 0, 17: 0, 20: 0, 25: 0, 13: 3, 21: 3, 22: 3, 3: 1, 6: 1, 8: 1, 10: 1,
                12: 1, 2: 2, 9: 2, 11: 2, 14: 2, 16: 2, 23: 2, 28: 4}
# item fields a player sees or the server uses for power; the rest (prices, flags, visuals) are listed as "other"
ITEM_KEY_FIELDS = ['Display', 'OverallQualityID', 'ItemLevel', 'RequiredLevel', 'ItemDelay', 'DmgVariance', 'Bonding',
                   'AllowableClass', 'ItemSet', 'InventoryType', 'RequiredSkill', 'RequiredSkillRank', 'MaxCount',
                   'ContainerSlots', 'StartQuestID', 'RequiredPVPRank', 'Stackable', 'MinFactionID', 'MinReputation']
LOOT_TABLES = ['creature_loot_template', 'gameobject_loot_template', 'item_loot_template', 'reference_loot_template',
               'disenchant_loot_template', 'pickpocketing_loot_template', 'skinning_loot_template',
               'fishing_loot_template']
WORLD_TABLES = LOOT_TABLES + [
    'classic_spell_info', 'classic_spell_cost', 'classic_spell_value', 'classic_spell_twin', 'trainer', 'trainer_spell',
    'creature_trainer', 'creature_template', 'creature', 'creature_queststarter', 'creature_questender',
    'gameobject_queststarter', 'gameobject_questender', 'gameobject', 'quest_template', 'npc_vendor',
    'creature_template_model', 'playercreateinfo_spell_custom', 'spell_learn_spell']
EPS = 1e-4


# ---------------------------------------------------------------- loading

def camel_to_snake(n):
    s = re.sub(r'(?<=[a-z0-9])([A-Z])', r'_\1', n)
    s = re.sub(r'(?<=[A-Z])([A-Z][a-z])', r'_\1', s)
    return s.lower()


class Build:
    """All decodable .db2 tables of one client build, decoded on first use."""

    def __init__(self, folder, sch, label):
        self.folder, self.sch, self.label = folder, sch, label
        self.files = {}
        for f in glob.glob(os.path.join(folder, '*.db2')):
            t = os.path.basename(f)[:-4]
            if t in sch.meta:
                self.files[t] = f
        self._cache, self.info = {}, {}

    def has(self, t):
        return t in self.files

    def __getitem__(self, t):
        if t not in self._cache:
            if t not in self.files:
                self._cache[t] = {}
            else:
                rows, info = wdc5.read_db2(self.files[t], self.sch, t)
                self._cache[t], self.info[t] = rows, info
        return self._cache[t]

    def by(self, t, key):
        """rows of table t grouped by field `key` (SpellID and the like)."""
        out = collections.defaultdict(list)
        for r in self[t].values():
            out[r[key]].append(r)
        return out


def same(a, b):
    if isinstance(a, float) or isinstance(b, float):
        try:
            return abs(float(a) - float(b)) <= EPS * max(1.0, abs(float(a)), abs(float(b)))
        except (TypeError, ValueError):
            return False
    if a is None:
        a = 0 if isinstance(b, int) else ''
    if b is None:
        b = 0 if isinstance(a, int) else ''
    return a == b


def row_diff(a, b, skip=()):
    return {k: (a.get(k), b.get(k)) for k in a if k not in skip and k in b and not same(a.get(k), b.get(k))}


# ---------------------------------------------------------------- helpers to describe things

class Names:
    def __init__(self, new, old, hot):
        self.spell = {}
        for src in (old['SpellName'] if old.has('SpellName') else {}, new['SpellName']):
            for k, r in src.items():
                self.spell[k] = r['Name']
        for r in hot.get('spell_name', []):
            self.spell.setdefault(r['ID'], r['Name'])
        self.item = {}
        for src in (old['ItemSparse'] if old.has('ItemSparse') else {}, new['ItemSparse']):
            for k, r in src.items():
                self.item[k] = r['Display']
        for r in hot.get('item_sparse', []):
            self.item.setdefault(r['ID'], r['Display'])
        self.item_extra = {}

    def s(self, i):
        return self.spell.get(i, '?')

    def i(self, i):
        return self.item.get(i) or self.item_extra.get(i) or '?'


def item_stats(r, rpp):
    """[(stat name, allocation, value at its ilvl)] the way Item::GetItemStatValue computes it."""
    out = []
    pts = 0.0
    q = r.get('OverallQualityID', 0)
    slot = INVTYPE_SLOT.get(r.get('InventoryType', 0))
    rp = rpp.get(r.get('ItemLevel', 0))
    if rp and slot is not None:
        key = {2: 'GoodF', 3: 'SuperiorF', 7: 'SuperiorF', 4: 'EpicF', 5: 'EpicF', 6: 'EpicF'}.get(q)
        if key:
            pts = rp.get('%s%d' % (key, slot + 1), 0.0)
    for i in range(1, 11):
        t, a = r.get('StatModifierBonusStat%d' % i, -1), r.get('StatPercentEditor%d' % i, 0)
        if t is None or t < 0 or not a:
            continue
        out.append((STAT.get(t, 'stat %d' % t), a, int(round(a * pts * 0.0001)) if pts else None))
    return out


def stats_text(st):
    return ', '.join('+%s %s' % (v if v is not None else '%d/10000' % a, n) for n, a, v in st) or '-'


def ms(v):
    if v is None:
        return '-'
    if v < 0:
        return 'forever'
    if v >= 3600000 and v % 60000 == 0:
        return '%gh' % (v / 3600000)
    if v >= 60000:
        return '%gm' % (v / 60000)
    return '%gs' % (v / 1000)


# ---------------------------------------------------------------- 1. build to build

def diff_builds(old, new, out):
    sec = out['build_diff'] = {'tables': [], 'not_compared': []}
    common = sorted(set(old.files) & set(new.files))
    sec['not_compared'] = sorted(set(new.files) - set(old.files))
    for t in common:
        if open(old.files[t], 'rb').read() == open(new.files[t], 'rb').read():
            sec['tables'].append(dict(table=t, identical=True, added=0, removed=0, changed=0,
                                      enc_old=0, enc_new=0))
            continue
        a, b = old[t], new[t]
        added = sorted(set(b) - set(a))
        removed = sorted(set(a) - set(b))
        changed = {}
        for k in set(a) & set(b):
            d = row_diff(a[k], b[k])
            if d:
                changed[k] = d
        fields = collections.Counter(f for d in changed.values() for f in d)
        sec['tables'].append(dict(table=t, identical=False, added=len(added), removed=len(removed),
                                  changed=len(changed), fields=fields.most_common(12),
                                  enc_old=old.info[t]['encrypted_records'], enc_new=new.info[t]['encrypted_records']))
        out['_raw'][t] = dict(added=added, removed=removed, changed=changed)


def items_build(old, new, names, out, hot_items):
    raw = out['_raw'].get('ItemSparse')
    if not raw:
        return
    rpp = new['RandPropPoints']
    a, b = old['ItemSparse'], new['ItemSparse']
    rows = []
    for k, d in raw['changed'].items():
        key = [f for f in d if f in ITEM_KEY_FIELDS or f.startswith('Stat')]
        sa, sb = item_stats(a[k], rpp), item_stats(b[k], rpp)
        if not key and sa == sb:
            continue
        r = b[k]
        changes = []
        for f in ('ItemLevel', 'RequiredLevel', 'OverallQualityID', 'ItemDelay', 'Bonding', 'AllowableClass',
                  'ItemSet', 'Display', 'MaxCount', 'Stackable', 'ContainerSlots', 'RequiredSkill',
                  'RequiredSkillRank', 'RequiredPVPRank', 'DmgVariance', 'StartQuestID'):
            if f in d:
                o, n = d[f]
                if f == 'OverallQualityID':
                    o, n = QUALITY[o], QUALITY[n]
                changes.append('%s %s -> %s' % (f, o, n))
        if sa != sb:
            changes.append('stats %s -> %s' % (stats_text(sa), stats_text(sb)))
        masked = k in hot_items
        rows.append(dict(id=k, name=r['Display'], quality=QUALITY[r['OverallQualityID']], ilvl=r['ItemLevel'],
                         req=r['RequiredLevel'], changes='; '.join(changes), masked_by_hotfix=masked,
                         ilvl_delta=d.get('ItemLevel', (0, 0))[1] - d.get('ItemLevel', (0, 0))[0]
                         if 'ItemLevel' in d else 0))
    rows.sort(key=lambda x: (-QUALITY.index(x['quality']), -x['ilvl'], x['id']))
    out['items_changed'] = rows
    out['items_added'] = [dict(id=k, name=b[k]['Display'], quality=QUALITY[b[k]['OverallQualityID']],
                               ilvl=b[k]['ItemLevel'], req=b[k]['RequiredLevel'],
                               stats=stats_text(item_stats(b[k], rpp))) for k in raw['added']]
    out['items_removed'] = [dict(id=k, name=a[k]['Display'], quality=QUALITY[a[k]['OverallQualityID']],
                                 ilvl=a[k]['ItemLevel']) for k in raw['removed']]


def spell_bundle(build, sid, by):
    """The rows a spell is made of, keyed for comparison."""
    out = {}
    nm = build['SpellName'].get(sid)
    if nm:
        out['name'] = nm['Name']
    for t, key in (('SpellMisc', 'SpellID'), ('SpellAuraOptions', 'SpellID'), ('SpellCategories', 'SpellID'),
                   ('SpellClassOptions', 'SpellID'), ('SpellEffect', 'SpellID'), ('SpellCooldowns', 'SpellID'),
                   ('SpellLevels', 'SpellID'), ('SpellPower', 'SpellID')):
        if t not in by:
            continue
        for r in by[t].get(sid, []):
            k2 = (t, r.get('DifficultyID', 0), r.get('EffectIndex', r.get('OrderIndex', 0)))
            out[k2] = r
    sp = build['Spell'].get(sid) if build.has('Spell') else None
    if sp:
        out['text'] = sp
    return out


COSMETIC = {'SpellIconFileDataID', 'ActiveIconFileDataID', 'SpellVisualScript', 'ActiveSpellVisualScript',
            'ContentTuningID', 'ShowFutureSpellPlayerConditionID'}


def load_attr_names(repo):
    """{(attributes field, bit): (name, client only)} from SharedDefines.h."""
    out = {}
    path = os.path.join(repo, 'src/server/game/Miscellaneous/SharedDefines.h')
    for line in open(path, encoding='utf8', errors='replace'):
        m = re.match(r'\s*SPELL_ATTR(\d+)_(\w+)\s*=\s*(0x[0-9A-Fa-f]+),?(.*)', line)
        if m:
            out[('Attributes%d' % (int(m.group(1)) + 1), int(m.group(3), 16))] = (
                m.group(2), 'client only' in m.group(4).lower() or m.group(2).startswith('UNK'))
    return out


def describe_spell_change(sid, oa, ob, new, cache):
    """Readable list of what moved in one spell."""
    dur = cache['dur']
    cast = cache['cast']
    rng = cache['rng']
    ch = []
    if oa.get('name') != ob.get('name'):
        ch.append('name %r -> %r' % (oa.get('name'), ob.get('name')))
    keys = sorted(set(k for k in list(oa) + list(ob) if isinstance(k, tuple)), key=str)
    for k in keys:
        t, diff_id, idx = k
        ra, rb = oa.get(k), ob.get(k)
        tag = t.replace('Spell', '') + (' e%d' % idx if t == 'SpellEffect' else '')
        if ra is None:
            ch.append('%s added' % tag)
            continue
        if rb is None:
            if t == 'SpellAuraOptions' and not (ra.get('ProcTypeMask1') or ra.get('ProcTypeMask2') or
                                                ra.get('ProcCharges') or ra.get('CumulativeAura')):
                continue  # an empty default row went away
            ch.append('%s removed' % tag)
            continue
        d = row_diff(ra, rb, skip=('ID',))
        for f, (x, y) in d.items():
            if f in COSMETIC:
                continue
            if f.startswith('Attributes'):
                names_ = []
                for bit in range(32):
                    m = 1 << bit
                    if ((x or 0) ^ (y or 0)) & m:
                        nm, client = cache['attr'].get((f, m), ('%s bit %#x' % (f, m), False))
                        if not client:
                            names_.append(('+' if (y or 0) & m else '-') + nm)
                if names_:
                    ch.append('flags ' + ' '.join(names_))
                continue
            if f == 'DurationIndex':
                ch.append('duration %s -> %s' % (ms(dur.get(x)), ms(dur.get(y))))
            elif f == 'CastingTimeIndex':
                ch.append('cast %s -> %s' % (ms(cast.get(x)), ms(cast.get(y))))
            elif f == 'RangeIndex':
                ch.append('range %s -> %s yd' % (rng.get(x, x), rng.get(y, y)))
            elif f in ('RecoveryTime', 'CategoryRecoveryTime', 'StartRecoveryTime'):
                ch.append('%s %s -> %s' % (f, ms(x), ms(y)))
            else:
                ch.append('%s %s %s -> %s' % (tag, f, x, y))
    ta, tb = oa.get('text'), ob.get('text')
    if ta and tb and ta.get('Description') != tb.get('Description'):
        ch.append('tooltip text changed')
    return ch


def spells_build(old, new, names, ctx, out):
    if not old.has('SpellName'):
        return
    tables = [t for t in ('SpellMisc', 'SpellAuraOptions', 'SpellCategories', 'SpellClassOptions', 'SpellEffect',
                          'SpellCooldowns', 'SpellLevels', 'SpellPower') if old.has(t) and new.has(t)]
    out['spell_tables_compared'] = ['SpellName', 'Spell'] + tables
    out['spell_tables_new_only'] = [t for t in ('SpellCooldowns', 'SpellLevels', 'SpellPower', 'SpellDuration',
                                                'SpellCastTimes', 'SpellRange') if not old.has(t)]
    bya = {t: old.by(t, 'SpellID') for t in tables}
    byb = {t: new.by(t, 'SpellID') for t in tables}
    touched = set()
    for t in tables + ['SpellName', 'Spell']:
        raw = out['_raw'].get(t)
        if not raw:
            continue
        if t in ('SpellName', 'Spell'):
            touched |= set(raw['changed'])
        else:
            for k in raw['changed']:
                touched.add(new[t][k]['SpellID'])
            for k in raw['added']:
                touched.add(new[t][k]['SpellID'])
            for k in raw['removed']:
                touched.add(old[t][k]['SpellID'])
    added = set(new['SpellName']) - set(old['SpellName'])
    removed = set(old['SpellName']) - set(new['SpellName'])
    touched -= added | removed
    rows = []
    for sid in touched:
        oa, ob = spell_bundle(old, sid, bya), spell_bundle(new, sid, byb)
        ch = describe_spell_change(sid, oa, ob, new, ctx)
        if not ch:
            continue
        rows.append(dict(id=sid, name=names.s(sid), cls=ctx['spell_class'].get(sid, ''),
                         player=sid in ctx['player_spells'], changes='; '.join(ch),
                         overridden=ctx['override_of'].get(sid, '')))
    rows.sort(key=lambda x: (not x['player'], x['cls'] or 'zz', x['name'], x['id']))
    out['spells_changed'] = rows
    out['spells_added'] = sorted(
        [dict(id=s, name=names.s(s), cls=ctx['spell_class'].get(s, ''), player=s in ctx['player_spells'],
              used=ctx['spell_use'].get(s, '')) for s in added], key=lambda x: (not x['player'], x['cls'] or 'zz', x['id']))
    out['spells_removed'] = sorted(
        [dict(id=s, name=old['SpellName'][s]['Name'], used=ctx['spell_use'].get(s, '')) for s in removed],
        key=lambda x: x['id'])


def talents_build(old, new, names, out):
    if not old.has('TraitDefinition'):
        return
    a, b = old['TraitDefinition'], new['TraitDefinition']
    rows = []
    for k in sorted(set(a) | set(b)):
        ra, rb = a.get(k), b.get(k)
        if ra and rb:
            d = row_diff(ra, rb)
            if d:
                rows.append(dict(id=k, spell=rb['SpellID'], name=names.s(rb['SpellID']),
                                 change='; '.join('%s %s -> %s' % (f, x, y) for f, (x, y) in d.items())))
        elif rb:
            rows.append(dict(id=k, spell=rb['SpellID'], name=names.s(rb['SpellID']), change='new talent definition'))
        else:
            rows.append(dict(id=k, spell=ra['SpellID'], name=names.s(ra['SpellID']), change='removed'))
    out['talents_changed'] = rows


# ---------------------------------------------------------------- 2. our server against the new client

def hotfix_overlay(new, hot, hot_cols, table_hash, names, out):
    """Hotfix DB rows replace the client record on the server (DB2StorageBase::LoadFromDB)."""
    sent = collections.defaultdict(set)
    for r in hot.get('hotfix_data', []):
        if r['Status'] == 1:
            sent[r['TableHash'] & 0xFFFFFFFF].add(r['RecordId'])
    res = []
    details = {}
    for db2 in sorted(new.files):
        ht = camel_to_snake(db2)
        rows = hot.get(ht)
        if not rows:
            continue
        cl = new[db2]
        idf = new.info[db2]['id_field']
        th = table_hash.get(db2, 0)
        same_n = diff_n = only_n = unsent = 0
        diffs = []
        for r in rows:
            rid = r.get(idf, r.get('ID'))
            c = cl.get(rid)
            if rid not in sent.get(th, ()):
                unsent += 1
            if c is None:
                only_n += 1
                continue
            d = {k: (c[k], r[k]) for k in c if k in r and k != idf and not same(c[k], r[k])}
            if d:
                diff_n += 1
                diffs.append((rid, r.get('VerifiedBuild'), d))
            else:
                same_n += 1
        res.append(dict(table=db2, hotfix_table=ht, rows=len(rows), same=same_n, differ=diff_n, hotfix_only=only_n,
                        not_sent=unsent, builds=dict(collections.Counter(r.get('VerifiedBuild') for r in rows)
                                                     .most_common(3))))
        details[db2] = diffs
    out['hotfix_tables'] = res
    out['_hotfix_diffs'] = details


def hotfix_items(new, old, hot, names, out):
    diffs = out['_hotfix_diffs'].get('ItemSparse', [])
    rpp = new['RandPropPoints']
    hs = {r['ID']: r for r in hot.get('item_sparse', [])}
    rows = []
    raw = out['_raw'].get('ItemSparse', {})
    changed_70 = raw.get('changed', {})
    for rid, vb, d in diffs:
        c, h = new['ItemSparse'][rid], hs[rid]
        key = [f for f in d if f in ITEM_KEY_FIELDS or f.startswith('Stat')]
        sc, sh = item_stats(c, rpp), item_stats(h, rpp)
        ch = []
        for f in ('ItemLevel', 'RequiredLevel', 'OverallQualityID', 'ItemDelay', 'Bonding', 'AllowableClass', 'ItemSet',
                  'Display', 'MaxCount', 'Stackable', 'DmgVariance'):
            if f in d:
                x, y = d[f]
                if f == 'OverallQualityID':
                    x, y = QUALITY[x], QUALITY[y]
                ch.append('%s client %s, server %s' % (f, x, y))
        if sc != sh:
            ch.append('stats client %s, server %s' % (stats_text(sc), stats_text(sh)))
        other = [f for f in d if f not in key]
        rows.append(dict(id=rid, name=c['Display'], quality=QUALITY[c['OverallQualityID']], ilvl=c['ItemLevel'],
                         hotfix_build=vb, key=bool(ch), changes='; '.join(ch) or 'only ' + ', '.join(other[:6]),
                         client_changed_since_old=rid in changed_70))
    rows.sort(key=lambda x: (not x['key'], -QUALITY.index(x['quality']), -x['ilvl']))
    out['hotfix_items_differ'] = rows


def hotfix_spells(new, names, ctx, out):
    rows = []
    for t in ('SpellName', 'SpellMisc', 'SpellEffect', 'SpellAuraOptions', 'SpellClassOptions', 'SpellLevels',
              'SpellPower', 'SpellCastTimes', 'SpellCastingRequirements', 'SpellReagents', 'SpellShapeshift',
              'SpellInterrupts', 'SpellEquippedItems', 'SkillLineAbility', 'TraitNode', 'TraitDefinition',
              'TraitDefinitionEffectPoints', 'TraitNodeEntry', 'TraitEdge', 'TraitNodeGroupXTraitNode',
              'TraitNodeXTraitNodeEntry', 'CurvePoint', 'QuestV2', 'ItemEffect', 'ItemXItemEffect', 'Item',
              'CreatureDisplayInfo', 'SpellLabel'):
        for rid, vb, d in out['_hotfix_diffs'].get(t, []):
            r = new[t].get(rid, {})
            sid = r.get('SpellID') or (rid if t == 'SpellName' else None) or r.get('Spell')
            what = '%s #%d' % (t, rid)
            if sid:
                what += ' (%s %d)' % (names.s(sid), sid)
            rows.append(dict(table=t, id=rid, hotfix_build=vb, what=what,
                             changes='; '.join('%s client %s, server %s' % (f, x, y) for f, (x, y) in list(d.items())[:8])))
    out['hotfix_other_differ'] = rows


def classic_info_vs_client(new, w, names, ctx, pending, out):
    dur, cast, rng = ctx['dur'], ctx['cast'], ctx['rng']
    misc = {}
    for r in new['SpellMisc'].values():
        if r.get('DifficultyID', 0) == 0:
            misc[r['SpellID']] = r
    cd = {}
    for r in new['SpellCooldowns'].values():
        if r.get('DifficultyID', 0) == 0:
            cd[r['SpellID']] = r
    rows = []
    for r in w.get('classic_spell_info', []):
        sid = r['spell_id']
        m = misc.get(sid)
        if m is None:
            continue
        c = cd.get(sid, {})
        # the player sees the longer of the spell and category cooldown; a NULL column keeps the client's value
        crec, ccat = c.get('RecoveryTime', 0), c.get('CategoryRecoveryTime', 0)
        srec = crec if r['recovery_ms'] is None else r['recovery_ms']
        scat = ccat if r['category_recovery_ms'] is None else r['category_recovery_ms']
        scd = None if r['recovery_ms'] is None and r['category_recovery_ms'] is None else max(srec, scat)
        cmp = [('cast', r['cast_ms'], cast.get(m['CastingTimeIndex'])),
               ('cooldown', scd, max(crec, ccat)),
               ('duration', r['duration_ms'], dur.get(m['DurationIndex'])),
               ('range', r['range_max'], rng.get(m['RangeIndex']))]
        diffs = []
        for nm, sv, cv in cmp:
            if sv is None or cv is None:
                continue
            if nm == 'duration' and sid in pending.get('duration_null', ()):
                continue
            if nm == 'range':
                if abs(float(sv) - float(cv)) > 0.01:
                    diffs.append('%s server %g yd, client %g yd' % (nm, sv, cv))
            elif int(sv) != int(cv):
                diffs.append('%s server %s, client %s' % (nm, ms(sv), ms(cv)))
        if diffs:
            rows.append(dict(id=sid, name=names.s(sid), cls=ctx['spell_class'].get(sid, ''),
                             player=sid in ctx['player_spells'], changes='; '.join(diffs)))
    rows.sort(key=lambda x: (not x['player'], x['cls'] or 'zz', x['name']))
    out['classic_info_differ'] = rows
    # mana costs
    power = collections.defaultdict(list)
    for r in new['SpellPower'].values():
        power[r['SpellID']].append(r)
    rows = []
    for r in w.get('classic_spell_cost', []):
        sid = r['spell_id']
        p = [x for x in power.get(sid, []) if x.get('PowerType', 0) == 0]
        if not p:
            continue
        p = p[0]
        sv, cv = (r['mana_cost'] or 0, r['mana_cost_pct'] or 0), (p.get('ManaCost', 0), p.get('PowerCostPct', 0.0))
        if sv[0] != cv[0] or abs(float(sv[1]) - float(cv[1])) > 0.01:
            rows.append(dict(id=sid, name=names.s(sid), cls=ctx['spell_class'].get(sid, ''),
                             player=sid in ctx['player_spells'],
                             changes='mana server %s%s, client %s%s' % (
                                 sv[0], ' (%g%%)' % sv[1] if sv[1] else '', cv[0], ' (%g%%)' % cv[1] if cv[1] else '')))
    rows.sort(key=lambda x: (not x['player'], x['cls'] or 'zz', x['name']))
    out['classic_cost_differ'] = rows
    # value rows on spells whose client effect moved since the old build
    moved = set()
    raw = out['_raw'].get('SpellEffect')
    if raw:
        for k in raw['changed']:
            r = new['SpellEffect'][k]
            if any(f in raw['changed'][k] for f in ('EffectBasePoints', 'EffectBonusCoefficient', 'BonusCoefficientFromAP',
                                                    'EffectRealPointsPerLevel', 'Variance', 'Coefficient')):
                moved.add((r['SpellID'], r['EffectIndex']))
    rows = []
    for r in w.get('classic_spell_value', []):
        if (r['spell_id'], r['effect_index']) in moved:
            rows.append(dict(id=r['spell_id'], name=names.s(r['spell_id']), effect=r['effect_index'],
                             cls=ctx['spell_class'].get(r['spell_id'], ''),
                             changes=raw['changed'][ctx['effect_key'][(r['spell_id'], r['effect_index'])]] and
                             '; '.join('%s %s -> %s' % (f, x, y) for f, (x, y) in
                                       raw['changed'][ctx['effect_key'][(r['spell_id'], r['effect_index'])]].items())))
    out['value_rows_masking_change'] = rows
    out['classic_counts'] = {t: len(w.get(t, [])) for t in ('classic_spell_info', 'classic_spell_cost',
                                                           'classic_spell_value', 'classic_spell_twin')}


def world_refs(new, hot, w, names, ctx, out):
    have_item = set(new['ItemSparse']) | {r['ID'] for r in hot.get('item_sparse', [])}
    item_db = new['Item']
    spawned_c = collections.Counter(r['id'] for r in w.get('creature', []))
    spawned_g = collections.Counter(r['id'] for r in w.get('gameobject', []))
    ctname = {r['entry']: r['name'] for r in w.get('creature_template', [])}
    # which loot ids are reachable from spawned creatures / objects (direct entry = template entry here)
    ref_users = collections.defaultdict(set)
    for t in LOOT_TABLES:
        for r in w.get(t, []):
            if r.get('ItemType', 0) == 1 or (r.get('Reference') or 0):
                ref = r.get('Item') if r.get('ItemType', 0) == 1 else r.get('Reference')
                ref_users[ref].add((t, r['Entry']))
    missing = collections.defaultdict(lambda: dict(rows=0, spawned_sources=set(), sources=set()))
    for t in LOOT_TABLES:
        for r in w.get(t, []):
            if r.get('ItemType', 0) != 0:
                continue
            it = r['Item']
            if it in have_item or it <= 0:
                continue
            m = missing[it]
            m['rows'] += 1
            srcs = [(t, r['Entry'])]
            if t == 'reference_loot_template':
                srcs = list(ref_users.get(r['Entry'], [])) or srcs
            for st, e in srcs:
                m['sources'].add((st, e))
                if (st == 'creature_loot_template' and spawned_c.get(e)) or \
                        (st == 'gameobject_loot_template' and e in ctx['go_loot_spawned']):
                    m['spawned_sources'].add((st, e))
    rows = []
    for it, m in missing.items():
        sp = sorted(m['spawned_sources'])
        ib = item_db.get(it, {})
        rows.append(dict(id=it, name=names.i(it), in_item_db2=bool(ib), item_class=ib.get('ClassID', ''),
                         loot_rows=m['rows'], spawned_sources=len(sp),
                         example=', '.join('%s %d' % (ctname.get(e, '?') if st.startswith('creature') else 'object', e)
                                           for st, e in sp[:3])))
    rows.sort(key=lambda x: (-x['spawned_sources'], -x['loot_rows']))
    out['loot_missing_items'] = rows
    # vendors
    vend = collections.defaultdict(set)
    for r in w.get('npc_vendor', []):
        if r['item'] > 0 and r.get('type', 1) == 1 and r['item'] not in have_item and spawned_c.get(r['entry']):
            vend[r['item']].add(r['entry'])
    out['vendor_missing_items'] = sorted(
        [dict(id=i, name=names.i(i), vendors=len(v), example=', '.join('%s %d' % (ctname.get(e, '?'), e)
                                                                       for e in sorted(v)[:3])) for i, v in vend.items()],
        key=lambda x: -x['vendors'])
    # quests the client knows about
    qt = {r['ID']: r for r in w.get('quest_template', [])}
    starters = collections.defaultdict(set)
    for r in w.get('creature_queststarter', []):
        starters[r['quest']].add(('npc', r['id'], bool(spawned_c.get(r['id']))))
    for r in w.get('gameobject_queststarter', []):
        starters[r['quest']].add(('object', r['id'], bool(spawned_g.get(r['id']))))
    item_start = collections.defaultdict(set)
    for src in (new['ItemSparse'], {r['ID']: r for r in hot.get('item_sparse', [])}):
        for k, r in src.items():
            if r.get('StartQuestID'):
                item_start[r['StartQuestID']].add(k)
    enders = collections.defaultdict(set)
    for r in w.get('creature_questender', []):
        enders[r['quest']].add(('npc', r['id'], bool(spawned_c.get(r['id']))))
    for r in w.get('gameobject_questender', []):
        enders[r['quest']].add(('object', r['id'], bool(spawned_g.get(r['id']))))
    client_q = set(new['QuestV2']) | {r['ID'] for r in hot.get('quest_v2', [])}
    no_tpl, no_giver, no_turnin, reward_missing = [], [], [], []
    junk = 0
    for q in sorted(client_q):
        t = qt.get(q)
        if not t:
            no_tpl.append(q)
            continue
        if t.get('Flags', 0) & 0x8000 and 0:  # auto-accept quests need no giver; kept for clarity
            pass
        title = t.get('LogTitle') or ''
        if not title or title.startswith('<') or re.search(r'\b(UNUSED|NYI|TEST|DEPRECATED|ZZOLD|REUSE)\b', title, re.I):
            junk += 1
            continue
        gs = starters.get(q, set())
        if not any(s[2] for s in gs) and not item_start.get(q):
            no_giver.append(dict(id=q, name=t.get('LogTitle', ''), level=ctx['quest_level'].get(q, ''),
                                 giver=', '.join('%s %d (no spawn)' % (k, e) for k, e, _ in sorted(gs)) or 'none'))
        es = enders.get(q, set())
        if not any(s[2] for s in es):
            no_turnin.append(q)
        for f in ['RewardItem%d' % i for i in range(1, 5)] + ['RewardChoiceItemID%d' % i for i in range(1, 7)]:
            it = t.get(f) or 0
            if it > 0 and it not in have_item:
                reward_missing.append(dict(quest=q, quest_name=t.get('LogTitle', ''), id=it, name=names.i(it)))
    out['quests'] = dict(client_quests=len(client_q), no_template=len(no_tpl), no_template_ids=no_tpl[:400],
                         no_template_min_id=min(no_tpl) if no_tpl else 0, placeholder=junk,
                         no_giver=no_giver, no_turnin=len(no_turnin), reward_missing=reward_missing)
    # trainers that are spawned
    trainer_of = collections.defaultdict(set)
    for r in w.get('creature_trainer', []):
        if spawned_c.get(r['CreatureID']):
            trainer_of[r['TrainerID']].add(r['CreatureID'])
    tr_type = {r['Id']: r for r in w.get('trainer', [])}
    known = set(new['SpellName']) | {r['ID'] for r in hot.get('spell_name', [])}
    levels = {}
    for r in new['SpellLevels'].values():
        if r.get('DifficultyID', 0) == 0:
            levels[r['SpellID']] = r
    taught = collections.defaultdict(set)
    bad_spell, bad_level = [], []
    for r in w.get('trainer_spell', []):
        tid, sid = r['TrainerId'], r['SpellId']
        if tid not in trainer_of:
            continue
        taught[sid].add(tid)
        for l in ctx['learns'].get(sid, ()):
            taught[l].add(tid)
        if sid not in known:
            bad_spell.append(dict(trainer=tid, spell=sid, name=names.s(sid), npcs=len(trainer_of[tid])))
            continue
        lv = levels.get(sid) or levels.get(next(iter(ctx['learns'].get(sid, [])), -1))
        if lv and lv.get('BaseLevel') and r['ReqLevel'] and lv['BaseLevel'] != r['ReqLevel'] and \
                tr_type.get(tid, {}).get('Type', 0) == 0:
            bad_level.append(dict(trainer=tid, spell=sid, name=names.s(sid), cls=ctx['spell_class'].get(sid, ''),
                                  trainer_level=r['ReqLevel'], client_level=lv['BaseLevel']))
    dedup = {}
    for b in bad_level:
        dedup.setdefault(b['spell'], dict(b, trainers=0))['trainers'] += 1
    out['trainer_unknown_spells'] = bad_spell
    out['trainer_level_differs'] = sorted(dedup.values(), key=lambda x: (x['cls'], x['name'], x['spell']))
    # class spells the client lists for a class skill line that no spawned trainer, talent, item or quest gives
    skip = ('Pet', 'Mounts', 'Companions', 'Engraving', 'Runes', 'Meditation', 'Explorer', 'Beast Training')
    class_lines = {k for k, r in new['SkillLine'].items()
                   if r.get('CategoryID') == 7 and not r['DisplayName'].startswith(skip)}
    auto = {r['Spell'] for r in w.get('playercreateinfo_spell_custom', [])}
    passive = {r['SpellID'] for r in new['SpellMisc'].values() if r.get('Attributes1', 0) & 0x40}
    untaught = []
    seen = set()
    for r in new['SkillLineAbility'].values():
        sid = r['Spell']
        if r['SkillLine'] not in class_lines or sid in seen or sid not in new['SpellName']:
            continue
        seen.add(sid)
        if r.get('AcquireMethod', 0) != 0 or sid in passive:
            continue  # learned on skill/level, or a passive
        if sid in taught or sid in ctx['talent_spells'] or sid in auto or sid in ctx['other_learn'] or \
                sid in ctx['triggered']:
            continue
        lv = levels.get(sid, {})
        if not 1 <= lv.get('BaseLevel', 0) <= ctx['level_cap']:
            continue
        untaught.append(dict(id=sid, name=names.s(sid), cls=ctx['spell_class'].get(sid, ''),
                             line=new['SkillLine'][r['SkillLine']]['DisplayName'], level=lv.get('BaseLevel', 0),
                             new_in_build=sid in ctx['spells_added'],
                             trainer_rows_unspawned=bool(sid in ctx['spell_use'] and 'trainer' in ctx['spell_use'][sid])))
    untaught.sort(key=lambda x: (x['cls'], x['level'], x['name']))
    out['class_spells_untaught'] = untaught
    # creatures whose display the client does not have
    have_disp = set(new['CreatureDisplayInfo']) | {r['ID'] for r in hot.get('creature_display_info', [])}
    disp = collections.defaultdict(list)
    for r in w.get('creature_template_model', []):
        disp[r['CreatureID']].append(r['CreatureDisplayID'])
    bad = []
    for e, ds in disp.items():
        if not spawned_c.get(e):
            continue
        if ds and all(d not in have_disp for d in ds):
            bad.append(dict(id=e, name=ctname.get(e, '?'), spawns=spawned_c[e], displays=', '.join(map(str, ds[:4]))))
    bad.sort(key=lambda x: -x['spawns'])
    out['creature_display_missing'] = bad
    # new items nobody can get on our server
    used_items = set()
    for t in LOOT_TABLES:
        for r in w.get(t, []):
            if r.get('ItemType', 0) == 0:
                used_items.add(r['Item'])
    used_items |= {r['item'] for r in w.get('npc_vendor', [])}
    for t in qt.values():
        for f in ['RewardItem%d' % i for i in range(1, 5)] + ['RewardChoiceItemID%d' % i for i in range(1, 7)] + \
                 ['StartItem']:
            used_items.add(t.get(f) or 0)
    out['new_items_unobtainable'] = [x for x in out.get('items_added', []) if x['id'] not in used_items]
    out['new_spells_unused'] = [x for x in out.get('spells_added', []) if x['player'] and not x['used']]


# ---------------------------------------------------------------- context shared by the sections

def build_ctx(new, old, hot, w, out):
    ctx = {'level_cap': out['level_cap'], 'attr': out['_attr']}
    ctx['dur'] = {k: r['Duration'] for k, r in new['SpellDuration'].items()}
    ctx['cast'] = {k: r['Base'] for k, r in new['SpellCastTimes'].items()}
    ctx['rng'] = {k: r.get('RangeMax1', r.get('RangeMax')) for k, r in new['SpellRange'].items()}
    cls = {}
    for b in (old, new):
        if b.has('SpellClassOptions'):
            for r in b['SpellClassOptions'].values():
                if r.get('SpellClassSet') in SPELL_FAMILY:
                    cls[r['SpellID']] = SPELL_FAMILY[r['SpellClassSet']]
    skip = ('Pet', 'Mounts', 'Companions', 'Engraving', 'Runes', 'Meditation', 'Explorer', 'Beast Training')
    class_lines = {k: r['DisplayName'] for k, r in new['SkillLine'].items()
                   if r.get('CategoryID') == 7 and not r['DisplayName'].startswith(skip)}
    player = set()
    for r in new['SkillLineAbility'].values():
        if r['SkillLine'] in class_lines:
            player.add(r['Spell'])
            cls.setdefault(r['Spell'], class_lines[r['SkillLine']])
    talent = set()
    for r in new['TraitDefinition'].values():
        for f in ('SpellID', 'OverridesSpellID', 'VisibleSpellID'):
            if r.get(f):
                talent.add(r[f])
    ctx['talent_spells'] = talent
    player |= talent
    learns = collections.defaultdict(set)
    for r in new['SpellEffect'].values():
        if r['Effect'] in (36, 57) and r.get('EffectTriggerSpell'):
            learns[r['SpellID']].add(r['EffectTriggerSpell'])
    ctx['learns'] = learns
    ctx['triggered'] = {r['EffectTriggerSpell'] for r in new['SpellEffect'].values()
                        if r.get('EffectTriggerSpell') and r['Effect'] not in (36, 57)}
    other = set()
    for r in new['ItemEffect'].values():
        if r.get('SpellID'):
            other.add(r['SpellID'])
            other |= learns.get(r['SpellID'], set())
    for q in w.get('quest_template', []):
        for f in ('RewardSpell', 'RewardDisplaySpell1', 'RewardDisplaySpell2', 'RewardDisplaySpell3'):
            if q.get(f):
                other.add(q[f])
                other |= learns.get(q[f], set())
    ctx['other_learn'] = other
    use = collections.defaultdict(list)
    for r in w.get('trainer_spell', []):
        use[r['SpellId']].append('trainer')
        for l in learns.get(r['SpellId'], ()):
            use[l].append('trainer')
            player.add(l)
        player.add(r['SpellId'])
    for t in talent:
        use[t].append('talent')
    for r in w.get('classic_spell_value', []):
        use[r['spell_id']].append('override')
    ctx['spell_use'] = {k: ', '.join(sorted(set(v))) for k, v in use.items()}
    ctx['player_spells'] = player
    ctx['spell_class'] = cls
    ov = collections.defaultdict(set)
    for t, lab in (('classic_spell_value', 'values'), ('classic_spell_info', 'cast/cd/duration'),
                   ('classic_spell_cost', 'mana'), ('classic_spell_twin', 'twin')):
        for r in w.get(t, []):
            ov[r['spell_id']].add(lab)
    ctx['override_of'] = {k: ', '.join(sorted(v)) for k, v in ov.items()}
    ctx['effect_key'] = {(r['SpellID'], r['EffectIndex']): k for k, r in new['SpellEffect'].items()
                         if r.get('DifficultyID', 0) == 0}
    ctx['spells_added'] = set(new['SpellName']) - set(old['SpellName']) if old.has('SpellName') else set()
    goloot = set()
    gt = {r['entry']: r for r in w.get('gameobject_template', [])}
    sg = {r['id'] for r in w.get('gameobject', [])}
    for e in sg:
        g = gt.get(e)
        if g and g.get('type') in (3, 25):
            goloot.add(g.get('Data1'))
    ctx['go_loot_spawned'] = goloot
    ctx['quest_level'] = {}
    return ctx


def parse_pending(paths):
    """Spell ids whose classic_spell_info duration the queued SQL already clears."""
    dn = set()
    for p in paths:
        files = sorted(glob.glob(os.path.join(p, '*'))) if os.path.isdir(p) else [p]
        for f in files:
            txt = open(f, encoding='utf8', errors='replace').read()
            for m in re.finditer(r"(UPDATE `classic_spell_info` SET `duration_ms` = NULL|DELETE FROM `classic_spell_info`)"
                                 r" WHERE `spell_id` IN \((.*?)\);", txt, re.S):
                body = re.sub(r'^\+', '', m.group(2), flags=re.M)
                body = re.sub(r'--[^\n]*', '', body)
                dn |= {int(x) for x in re.findall(r'\b\d+\b', body)}
    return {'duration_null': dn}


def load_sql(path, tables, cache_db):
    con = sqlite3.connect(cache_db)
    cols = sqldump.load_dump(path, set(tables), con)
    out = {}
    for t in tables:
        if not cols.get(t):
            continue
        cur = con.execute('SELECT * FROM "%s"' % t)
        names = [c[0] for c in cur.description]
        out[t] = [dict(zip(names, r)) for r in cur]
    con.close()
    return out, cols


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--repo', default='.')
    ap.add_argument('--old', required=True)
    ap.add_argument('--new', required=True)
    ap.add_argument('--old-label', default='old')
    ap.add_argument('--new-label', default='new')
    ap.add_argument('--hotfixes')
    ap.add_argument('--world')
    ap.add_argument('--pending-sql', nargs='*', default=[])
    ap.add_argument('--level-cap', type=int, default=60)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    sch = wdc5.Schemas(a.repo)
    old, new = Build(a.old, sch, a.old_label), Build(a.new, sch, a.new_label)
    out = {'_raw': {}, '_attr': load_attr_names(a.repo), 'old_label': a.old_label, 'new_label': a.new_label, 'level_cap': a.level_cap,
           'generated': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d %H:%M UTC')}
    hot, w = {}, {}
    cache = os.path.join(a.out, 'dump-cache.sqlite')
    if a.hotfixes:
        want = ['hotfix_data'] + [camel_to_snake(t) for t in new.files]
        hot, _ = load_sql(a.hotfixes, want, cache)
    if a.world:
        w, _ = load_sql(a.world, WORLD_TABLES + ['gameobject_template'], cache)
    if os.path.exists(cache):
        os.remove(cache)
    table_hash = {}
    for t, f in new.files.items():
        with open(f, 'rb') as fh:
            d = fh.read(160)
        table_hash[t] = int.from_bytes(d[136 + 16:136 + 20], 'little')
    names = Names(new, old, hot)
    pending = parse_pending(a.pending_sql)
    diff_builds(old, new, out)
    ctx = build_ctx(new, old, hot, w, out)
    hot_items = {r['ID'] for r in hot.get('item_sparse', [])}
    items_build(old, new, names, out, hot_items)
    spells_build(old, new, names, ctx, out)
    talents_build(old, new, names, out)
    if hot:
        hotfix_overlay(new, hot, None, table_hash, names, out)
        hotfix_items(new, old, hot, names, out)
        hotfix_spells(new, names, ctx, out)
    if w:
        classic_info_vs_client(new, w, names, ctx, pending, out)
        world_refs(new, hot, w, names, ctx, out)
    out['encrypted'] = [dict(table=t, records=new.info[t]['encrypted_records'], sections=new.info[t]['encrypted_sections'])
                        for t in sorted(new.info) if new.info[t]['encrypted_records']]
    out['pending_duration_rows'] = len(pending['duration_null'])
    out.pop('_raw')
    out.pop('_attr')
    out.pop('_hotfix_diffs', None)
    with open(os.path.join(a.out, 'report.json'), 'w') as f:
        json.dump(out, f, indent=1, default=lambda o: sorted(o) if isinstance(o, set) else str(o))
    for k, v in out.items():
        if isinstance(v, list) and v and isinstance(v[0], dict):
            write_tsv(os.path.join(a.out, k + '.tsv'), v)
    import render
    open(os.path.join(a.out, 'report.html'), 'w').write(render.render(out))
    print('wrote', a.out)


def write_tsv(path, rows):
    keys = list(rows[0])
    with open(path, 'w') as f:
        f.write('\t'.join(keys) + '\n')
        for r in rows:
            f.write('\t'.join(str(r.get(k, '')).replace('\t', ' ') for k in keys) + '\n')


if __name__ == '__main__':
    main()
