#!/usr/bin/env python3
"""Build hotfixes.item_sparse rows for vanilla items that the 70532 client has in Item.db2 but not in ItemSparse.db2.

The server makes an item template only from an ItemSparse row (ObjectMgr::LoadItemTemplates), so such items do not exist in game:
quest rewards come out blank, vendor rows are dropped. This writes the missing rows from the vMaNGOS 1.12 item data, the way the
70532 data holds the items it has:
  - base stats and resistances as stat allocations: value = allocation * RandPropPoints(item level, quality, slot) / 10000 (rounded),
    the same formula the server (Item::GetItemStatValue) and the client tooltip use;
  - equip spells that are plain stats in vanilla (crit, hit, dodge, parry, block, defense, attack power, spell damage and healing,
    mp5, resistances) as stats too, with the 70532 rating per vanilla 1% (14 crit, 10 hit, 12 dodge, 15 parry, 5 block);
  - flags and the other modern columns from the 70532 items of the same class, subclass and slot.
Other equip, use and chance-on-hit spells are listed in the report. With --effects (the client's ItemEffect, ItemXItemEffect and
SpellEffect) they are written as item_effect / item_x_item_effect rows, except for items the client links spells to itself: those
keep the client's spells, and a vanilla equip bonus of the same kind as a linked equip spell gives no stat.

--validate rebuilds every item that both the 70532 ItemSparse and vMaNGOS have and prints how many columns and stats come out the
same, which is the check that the conversion matches the client's own items.

usage:
  gen_hotfix_items.py --validate <ItemSparse.db2> <RandPropPoints.db2> <vmangos db> [--effects <ItemEffect.db2> <ItemXItemEffect.db2> <SpellEffect.db2>]
  gen_hotfix_items.py <ItemSparse.db2> <RandPropPoints.db2> <vmangos db> <ids file> <out.sql> <first hotfix_data Id> <first UniqueId> [VerifiedBuild]
      [--effects <ItemEffect.db2> <ItemXItemEffect.db2> <SpellEffect.db2>] [--first-effect-id <id>]
"""
import collections
import os
import sys

import pymysql

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import itemsparse  # noqa: E402

SOCK = os.environ.get('MYSQL_SOCK', '/run/mysqld/mysqld.sock')
TABLE_HASH_ITEM_SPARSE = 2442913102
TABLE_HASH_ITEM_EFFECT = 1073915313
TABLE_HASH_ITEM_X_ITEM_EFFECT = 13330255
EFFECT_COLUMNS = ['ID', 'LegacySlotIndex', 'TriggerType', 'Charges', 'CoolDownMSec', 'CategoryCoolDownMSec', 'SpellCategoryID', 'SpellID',
                  'ChrSpecializationID', 'PlayerConditionID']

# vMaNGOS stat_typeN keeps the modern ITEM_MOD ids for these
BASE_STATS = {0: 0, 1: 1, 3: 3, 4: 4, 5: 5, 6: 6, 7: 7}
# vMaNGOS resistance columns -> ITEM_MOD_*_RESISTANCE
RESISTANCES = {'fire_res': 51, 'frost_res': 52, 'holy_res': 53, 'shadow_res': 54, 'nature_res': 55, 'arcane_res': 56}
SCHOOL_RESIST = {2: 53, 4: 51, 8: 55, 16: 52, 32: 54, 64: 56}
SCHOOL_DAMAGE = {2: 84, 4: 85, 8: 86, 16: 87, 32: 88, 64: 89}
SPELL_SCHOOLS = 126


def equip_aura_stat(aura, misc, value):
    """(stat type, amount) for an equip aura that the 70532 data stores as a stat, or None."""
    if aura in (52, 57, 71):            # melee crit, spell crit, spell crit by school: crit rating, 14 per 1%
        return 32, value * 14
    if aura == 54:                      # melee hit: hit rating, 10 per 1%
        return 31, value * 10
    if aura == 55:                      # spell hit
        return 31, value * 10
    if aura == 49:                      # dodge, 12 per 1%
        return 13, value * 12
    if aura == 47:                      # parry, 15 per 1%
        return 14, value * 15
    if aura == 51:                      # block chance, 5 per 1%
        return 15, value * 5
    if aura == 30 and misc == 95:       # defense skill
        return 12, value
    if aura == 85:                      # mp5
        return 43, value
    if aura == 99:                      # attack power
        return 38, value
    if aura == 124:                     # ranged attack power
        return 39, value
    if aura == 158:                     # block value
        return 48, value
    if aura == 135:                     # healing
        return 41, value
    if aura == 13 and misc & SPELL_SCHOOLS == SPELL_SCHOOLS:   # damage of all spell schools
        return 42, value
    if aura == 13 and misc in SCHOOL_DAMAGE:
        return SCHOOL_DAMAGE[misc], value
    if aura == 22 and misc == 1:        # armor
        return 50, value
    if aura == 22 and misc in SCHOOL_RESIST:
        return SCHOOL_RESIST[misc], value
    return None


def aura_family(aura):
    """Auras that become the same rating count as one when matching a vanilla equip spell to a client linked one."""
    if aura in (52, 57, 71):
        return 'crit'
    if aura in (54, 55):
        return 'hit'
    return aura


def prop_index(inv, subclass):
    if inv in (1, 5, 7, 15, 17, 20, 25):
        return 0
    if inv == 26:
        return 3 if subclass == 19 else 0
    if inv in (13, 21, 22):
        return 3
    if inv in (3, 6, 8, 10, 12):
        return 1
    if inv in (2, 9, 11, 14, 16, 23):
        return 2
    if inv == 28:
        return 4
    return None


def rand_prop_points(rpp, ilvl, quality, inv, subclass):
    p = prop_index(inv, subclass)
    if p is None or ilvl not in rpp:
        return 0
    key = {2: 'Good', 3: 'Superior', 4: 'Epic', 5: 'Epic', 6: 'Epic', 7: 'Superior'}.get(quality)
    return rpp[ilvl][key][p] if key else 0


class Converter:
    def __init__(self, sparse_path, rpp_path, vmdb, effect_paths=None):
        self.sparse = itemsparse.read_sparse(sparse_path)
        self.rpp = itemsparse.read_randproppoints(rpp_path)
        # effect_paths: ItemEffect.db2, ItemXItemEffect.db2, SpellEffect.db2 of the client
        self.client_effects = itemsparse.read_item_effects(*effect_paths[:2]) if effect_paths else {}
        self.spell_auras = itemsparse.read_spell_auras(effect_paths[2]) if effect_paths else {}
        db = pymysql.connect(unix_socket=SOCK, user='root', database=vmdb, cursorclass=pymysql.cursors.DictCursor)
        cur = db.cursor()
        cur.execute("SELECT t.* FROM item_template t JOIN (SELECT entry, MAX(patch) p FROM item_template WHERE patch <= 10 GROUP BY entry) m "
                    "ON m.entry = t.entry AND m.p = t.patch")
        self.vm = {r['entry']: r for r in cur.fetchall()}
        cur.execute("SELECT t.* FROM spell_template t JOIN (SELECT entry, MAX(build) b FROM spell_template WHERE build <= 5875 GROUP BY entry) m "
                    "ON m.entry = t.entry AND m.b = t.build")
        self.spells = {r['entry']: r for r in cur.fetchall()}
        # modern columns without a vanilla source: the most common value among the 70532 items of the same class, subclass and slot
        self.groups = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
        self.race_masks = collections.defaultdict(collections.Counter)
        self.class_masks = collections.defaultdict(collections.Counter)
        for id_, row in self.sparse.items():
            v = self.vm.get(id_)
            if not v:
                continue
            for keyset in self._keys(v):
                for col in ('Flags2', 'Flags3', 'Flags4', 'Flags5', 'DmgVariance', 'QualityModifier', 'BagFamily'):
                    self.groups[keyset][col][row[col]] += 1
            # race and class masks: the 70532 value the client's own items use for the same vanilla mask
            self.race_masks[v['allowable_race']][(row['AllowableRace1'], row['AllowableRace2'])] += 1
            self.class_masks[v['allowable_class']][row['AllowableClass']] += 1

    @staticmethod
    def _keys(v):
        return [(v['class'], v['subclass'], v['inventory_type']), (v['class'], v['subclass']), (v['class'],)]

    def _group_value(self, v, col, default):
        for keyset in self._keys(v):
            c = self.groups.get(keyset, {}).get(col)
            if c:
                return c.most_common(1)[0][0]
        return default

    def _race_mask(self, vanilla):
        c = self.race_masks.get(vanilla)
        if c:
            return c.most_common(1)[0][0]
        if vanilla in (-1, 0) or vanilla & 0xFF == 0xFF:
            return -1, -1
        if vanilla & 77 == vanilla:      # alliance races only
            return self.race_masks[77].most_common(1)[0][0]
        if vanilla & 178 == vanilla:     # horde races only
            return self.race_masks[178].most_common(1)[0][0]
        return vanilla, 0

    def _class_mask(self, vanilla):
        c = self.class_masks.get(vanilla)
        if c:
            return c.most_common(1)[0][0]
        return -1 if vanilla in (-1, 0) or vanilla & 0x5DF == 0x5DF else vanilla

    def equip_auras(self, v):
        """[(aura, misc, value, spell id)] of the item's equip spells (trigger 1)."""
        out = []
        for n in range(1, 6):
            sid, trig = v['spellid_%d' % n], v['spelltrigger_%d' % n]
            if not sid or trig != 1 or sid not in self.spells:
                continue
            sp = self.spells[sid]
            for e in (1, 2, 3):
                if sp['effect%d' % e] == 6:
                    out.append((sp['effectApplyAuraName%d' % e], sp['effectMiscValue%d' % e],
                                sp['effectBasePoints%d' % e] + sp['effectDieSides%d' % e], sid))
        return out

    def stats(self, v, linked_auras=None):
        """({stat type: amount}, [spells not turned into stats]).

        linked_auras: for an item whose spells the client links itself (ItemXItemEffect), the aura types of those linked equip spells.
        A vanilla equip spell with only those auras is the client's own spell now and gives no stat, and no spell is listed."""
        st = collections.OrderedDict()
        for n in range(1, 11):
            t, val = v['stat_type%d' % n], v['stat_value%d' % n]
            if val and t in BASE_STATS:
                st[BASE_STATS[t]] = st.get(BASE_STATS[t], 0) + val
        for col, t in RESISTANCES.items():
            if v[col]:
                st[t] = st.get(t, 0) + v[col]
        left = []
        auras = self.equip_auras(v)
        by_spell = collections.defaultdict(list)
        for a in auras:
            by_spell[a[3]].append(a)
        for sid, effs in by_spell.items():
            if linked_auras is not None and all(aura_family(a) in linked_auras for a, _, _, _ in effs):
                continue
            conv = [equip_aura_stat(a, m, val) for a, m, val, _ in effs]
            if all(conv):
                # damage + healing of all schools is spell power in this data
                types = [c[0] for c in conv]
                if 42 in types and 41 in types:
                    conv = [(45, dict(conv)[42])]
                if 38 in types and 39 in types:
                    # attack power for melee and ranged in one spell is plain attack power
                    conv = [c for c in conv if c[0] != 39]
                elif 39 in types:
                    # ranged attack power alone is stored at three times the vanilla value (Mithril Heavy-bore Rifle 14 -> 42)
                    conv = [(39, c[1] * 3) if c[0] == 39 else c for c in conv]
                for t, val in conv:
                    st[t] = st.get(t, 0) + val
            else:
                left.append(sid)
        for n in range(1, 6):
            sid, trig = v['spellid_%d' % n], v['spelltrigger_%d' % n]
            if sid and (trig != 1 or sid not in by_spell):
                left.append(sid)
        return st, ([] if linked_auras is not None else left)

    def linked_auras(self, id_):
        """Aura types of the equip spells the client links to the item, None when it links none."""
        if id_ not in self.client_effects:
            return None
        return {aura_family(a) for e in self.client_effects[id_] if e['TriggerType'] == 1 for a in self.spell_auras.get(e['SpellID'], ())}

    def build(self, id_):
        v = self.vm[id_]
        quality, ilvl, inv = v['quality'], v['item_level'], v['inventory_type']
        row = {c: 0 for c in itemsparse.COLUMNS}
        row.update({c: '' for c in itemsparse.STRINGS})
        row.update(ID=id_, Display=v['name'], Description=v['description'] or '')
        for n in range(1, 11):
            row['StatModifierBonusStat%d' % n] = -1
            row['StatPercentageOfSocket%d' % n] = 0.0
        st, left = self.stats(v, self.linked_auras(id_))
        pts = rand_prop_points(self.rpp, ilvl, quality, inv, v['subclass'])
        unplaced = {}
        for n, (t, val) in enumerate(st.items(), 1):
            if n > 10 or not pts:
                unplaced[t] = val
                continue
            row['StatModifierBonusStat%d' % n] = t
            row['StatPercentEditor%d' % n] = int(round(val * 10000 / pts))
        row.update(
            Stackable=v['stackable'], MaxCount=v['max_count'],
            MinReputation=v['required_reputation_rank'], RequiredAbility=v['required_spell'],
            AllowableRace1=self._race_mask(v['allowable_race'])[0], AllowableRace2=self._race_mask(v['allowable_race'])[1],
            SellPrice=v['sell_price'], BuyPrice=v['buy_price'], VendorStackCount=v['buy_count'] or 1,
            PriceVariance=1.0, PriceRandomValue=1.0,
            Flags1=v['flags'],
            DurationInInventory=v['duration'], BagFamily=v['bag_family'], StartQuestID=v['start_quest'], LanguageID=v['page_language'],
            ItemRange=float(v['range_mod']),
            ItemSet=v['set_id'], LockID=v['lock_id'], PageID=v['page_text'], ItemDelay=v['delay'],
            MinFactionID=v['required_reputation_faction'], RequiredSkillRank=v['required_skill_rank'], RequiredSkill=v['required_skill'],
            ItemLevel=ilvl,
            AllowableClass=self._class_mask(v['allowable_class']),
            SheatheType=v['sheath'], Material=max(v['material'], 0), PageMaterialID=v['page_material'], Bonding=v['bonding'],
            DamageDamageType=v['dmg_type1'], ContainerSlots=v['container_slots'], RequiredPVPRank=v['required_honor_rank'],
            RequiredLevel=v['required_level'], InventoryType=inv, OverallQualityID=quality, AmmunitionType=v['ammo_type'],
            InstanceBound=v['map_bound'], ZoneBound1=v['area_bound'])
        for col, default in (('Flags2', 0x6004), ('Flags3', 0), ('Flags4', 0), ('Flags5', 0), ('QualityModifier', 0.0)):
            row[col] = self._group_value(v, col, default)
        if v['class'] == 2:
            row['DmgVariance'] = self._group_value(v, 'DmgVariance', 0.0)
        if not row['BagFamily']:
            row['BagFamily'] = self._group_value(v, 'BagFamily', 0)
        return row, left, unplaced

    def effects(self, id_, spells):
        """ItemEffect rows for the vMaNGOS spell slots that hold one of spells (the spells build() did not turn into stats)."""
        v = self.vm[id_]
        out = []
        for n in range(1, 6):
            sid = v['spellid_%d' % n]
            if not sid or sid not in spells:
                continue
            cooldown, category_cooldown = v['spellcooldown_%d' % n], v['spellcategorycooldown_%d' % n]
            out.append({'LegacySlotIndex': n - 1, 'TriggerType': v['spelltrigger_%d' % n], 'Charges': v['spellcharges_%d' % n],
                        'CoolDownMSec': cooldown if cooldown > 0 else -1,
                        'CategoryCoolDownMSec': category_cooldown if category_cooldown > 0 else -1,
                        'SpellCategoryID': v['spellcategory_%d' % n], 'SpellID': sid, 'ChrSpecializationID': 0, 'PlayerConditionID': 0})
        return out


COMPARE = ['Display', 'Description', 'OverallQualityID', 'ItemLevel', 'RequiredLevel', 'InventoryType', 'AllowableClass', 'Bonding',
           'SellPrice', 'BuyPrice', 'Stackable', 'MaxCount', 'ItemDelay', 'Material', 'SheatheType', 'StartQuestID', 'ItemSet',
           'Flags1', 'Flags2', 'Flags3', 'Flags4', 'Flags5', 'DmgVariance', 'DamageDamageType', 'BagFamily', 'QualityModifier',
           'AllowableRace1', 'AllowableRace2', 'VendorStackCount', 'RequiredSkill', 'RequiredSkillRank', 'LanguageID', 'ItemRange']


def stat_values(conv, row, subclass):
    pts = rand_prop_points(conv.rpp, row['ItemLevel'], row['OverallQualityID'], row['InventoryType'], subclass)
    out = collections.Counter()
    for n in range(1, 11):
        t, a = row['StatModifierBonusStat%d' % n], row['StatPercentEditor%d' % n]
        if t >= 0 and a:
            out[t] += a * pts / 10000
    return {t: int(round(x)) for t, x in out.items()}


def validate(conv):
    shared = sorted(i for i in conv.sparse if i in conv.vm)
    same = collections.Counter()
    stat_ok = 0
    stat_bad = collections.Counter()
    for i in shared:
        mine, _, _ = conv.build(i)
        theirs = conv.sparse[i]
        for c in COMPARE:
            a, b = mine[c], theirs[c]
            if isinstance(a, float) or isinstance(b, float):
                same[c] += abs(float(a) - float(b)) < 1e-3
            else:
                same[c] += a == b
        sub = conv.vm[i]['subclass']
        ms, ts = stat_values(conv, mine, sub), stat_values(conv, theirs, sub)
        if ms == ts:
            stat_ok += 1
        else:
            for t in set(ms) | set(ts):
                if ms.get(t) != ts.get(t):
                    stat_bad[t] += 1
    print('items in both:', len(shared))
    for c in COMPARE:
        print('  %-20s %5.1f%% same' % (c, 100.0 * same[c] / len(shared)))
    print('  %-20s %5.1f%% same (all stat values)' % ('stats', 100.0 * stat_ok / len(shared)))
    print('  stat types that differ (items):', stat_bad.most_common(20))


def sql_value(v):
    if isinstance(v, str):
        return "'" + v.replace('\\', '\\\\').replace("'", "\\'") + "'"
    if isinstance(v, float):
        return repr(round(v, 6))
    return str(v)


def generate(conv, ids, out, first_hotfix_id, first_unique_id, build, first_effect_id):
    rows, notes, effects = [], [], []
    for i in ids:
        if i in conv.sparse:
            notes.append('-- %d: already in the 70532 ItemSparse, skipped' % i)
            continue
        if i not in conv.vm:
            notes.append('-- %d: not in vMaNGOS, skipped' % i)
            continue
        row, left, unplaced = conv.build(i)
        row['VerifiedBuild'] = build
        rows.append(row)
        extra = []
        if i in conv.client_effects:
            extra.append('spells from the client ItemXItemEffect: %s' % ','.join(str(e['SpellID']) for e in conv.client_effects[i]))
        elif left and conv.client_effects:
            for e in conv.effects(i, left):
                e.update(ID=first_effect_id + len(effects), ItemID=i)
                effects.append(e)
            extra.append('ItemEffect rows for spells: %s' % ','.join(map(str, left)))
        elif left:
            extra.append('spells not converted (need ItemEffect rows): %s' % ','.join(map(str, left)))
        if unplaced:
            extra.append('stats not placed: %s' % unplaced)
        notes.append('-- %d %s%s' % (i, row['Display'], ('; ' + '; '.join(extra)) if extra else ''))
    cols = itemsparse.COLUMNS + ['VerifiedBuild']
    with open(out, 'w', encoding='utf8') as f:
        f.write('-- Generated by tools/classic_items/gen_hotfix_items.py from vMaNGOS 1.12 item data (see the tool for the method).\n')
        f.write('\n'.join(notes) + '\n\n')
        if not rows:
            return
        ids_sql = ','.join(str(r['ID']) for r in rows)
        f.write('DELETE FROM `item_sparse` WHERE `ID` IN (%s);\n' % ids_sql)
        f.write('INSERT INTO `item_sparse` (%s) VALUES\n' % ','.join('`%s`' % c for c in cols))
        f.write(',\n'.join('(' + ','.join(sql_value(r[c]) for c in cols) + ')' for r in rows) + ';\n\n')
        f.write('DELETE FROM `hotfix_data` WHERE `TableHash` = %d AND `RecordId` IN (%s);\n' % (TABLE_HASH_ITEM_SPARSE, ids_sql))
        f.write('INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES\n')
        f.write(',\n'.join('(%d,%d,%d,%d,1,%d)' % (first_hotfix_id + k, first_unique_id + k, TABLE_HASH_ITEM_SPARSE, r['ID'], build)
                           for k, r in enumerate(rows)) + ';\n')
        if not effects:
            return
        # one ItemEffect and one ItemXItemEffect record per spell, same id for both; their hotfix_data follow the item rows
        effect_ids = ','.join(str(e['ID']) for e in effects)
        f.write('\nDELETE FROM `item_effect` WHERE `ID` IN (%s);\n' % effect_ids)
        f.write('INSERT INTO `item_effect` (%s) VALUES\n' % ','.join('`%s`' % c for c in EFFECT_COLUMNS + ['VerifiedBuild']))
        f.write(',\n'.join('(' + ','.join(sql_value(e[c]) for c in EFFECT_COLUMNS) + ',%d)' % build for e in effects) + ';\n')
        f.write('DELETE FROM `item_x_item_effect` WHERE `ID` IN (%s);\n' % effect_ids)
        f.write('INSERT INTO `item_x_item_effect` (`ID`,`ItemEffectID`,`ItemID`,`VerifiedBuild`) VALUES\n')
        f.write(',\n'.join('(%d,%d,%d,%d)' % (e['ID'], e['ID'], e['ItemID'], build) for e in effects) + ';\n')
        f.write('DELETE FROM `hotfix_data` WHERE (`TableHash` = %d OR `TableHash` = %d) AND `RecordId` IN (%s);\n'
                % (TABLE_HASH_ITEM_EFFECT, TABLE_HASH_ITEM_X_ITEM_EFFECT, effect_ids))
        f.write('INSERT INTO `hotfix_data` (`Id`,`UniqueId`,`TableHash`,`RecordId`,`Status`,`VerifiedBuild`) VALUES\n')
        k = len(rows)
        lines = []
        for e in effects:
            for table_hash in (TABLE_HASH_ITEM_EFFECT, TABLE_HASH_ITEM_X_ITEM_EFFECT):
                lines.append('(%d,%d,%d,%d,1,%d)' % (first_hotfix_id + k, first_unique_id + k, table_hash, e['ID'], build))
                k += 1
        f.write(',\n'.join(lines) + ';\n')


def main():
    args = sys.argv[1:]
    effect_paths = None
    if '--effects' in args:
        k = args.index('--effects')
        effect_paths = args[k + 1:k + 4]
        del args[k:k + 4]
    first_effect_id = 9910001
    if '--first-effect-id' in args:
        k = args.index('--first-effect-id')
        first_effect_id = int(args[k + 1])
        del args[k:k + 2]
    if args[0] == '--validate':
        validate(Converter(*args[1:4], effect_paths=effect_paths))
        return
    sparse, rpp, vmdb, ids_file, out, first, first_unique = args[0:7]
    build = int(args[7]) if len(args) > 7 else 70532
    ids = [int(x) for x in open(ids_file).read().split() if x.strip()]
    generate(Converter(sparse, rpp, vmdb, effect_paths), ids, out, int(first), int(first_unique), build, first_effect_id)


if __name__ == '__main__':
    main()
