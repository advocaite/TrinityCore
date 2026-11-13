"""Read the 70532 ItemSparse.db2 (WDC5, sparse) into dicts keyed by the hotfixes.item_sparse column names, and RandPropPoints.db2.

ItemSparse records: five inline zero-terminated strings (Description, Display3, Display2, Display1, Display), then the fixed fields
in the order of ItemSparseLoadInfo (src/server/game/DataStores/DB2LoadInfo.h). Only section 0 holds records; the other sections are
single placeholder records.
"""
import struct

STRINGS = ['Description', 'Display3', 'Display2', 'Display1', 'Display']
FIELDS = ([('ExpansionID', 'i'), ('DmgVariance', 'f'), ('LimitCategory', 'i'), ('DurationInInventory', 'i'), ('QualityModifier', 'f'),
           ('BagFamily', 'i'), ('StartQuestID', 'i'), ('LanguageID', 'i'), ('ItemRange', 'f')]
          + [('StatPercentageOfSocket%d' % i, 'f') for i in range(1, 11)]
          + [('StatPercentEditor%d' % i, 'i') for i in range(1, 11)]
          + [('StatModifierBonusStat%d' % i, 'i') for i in range(1, 11)]
          + [(n, 'i') for n in ('Stackable', 'MaxCount', 'MinReputation', 'RequiredAbility', 'AllowableRace1', 'AllowableRace2',
                                'SellPrice', 'BuyPrice', 'VendorStackCount')]
          + [('PriceVariance', 'f'), ('PriceRandomValue', 'f')]
          + [(n, 'i') for n in ('Flags1', 'Flags2', 'Flags3', 'Flags4', 'Flags5', 'FactionRelated', 'ModifiedCraftingReagentItemID',
                                'ContentTuningID', 'PlayerLevelToItemLevelCurveID', 'ItemLevelOffsetCurveID', 'ItemLevelOffsetItemLevel',
                                'ItemSquishEraID')]
          + [(n, 'H') for n in ('ItemNameDescriptionID', 'RequiredTransmogHoliday', 'RequiredHoliday', 'GemProperties',
                                'SocketMatchEnchantmentId', 'TotemCategoryID', 'InstanceBound', 'ZoneBound1', 'ZoneBound2', 'ItemSet',
                                'LockID', 'PageID', 'ItemDelay', 'MinFactionID', 'RequiredSkillRank', 'RequiredSkill', 'ItemLevel')]
          + [('AllowableClass', 'h')]
          + [(n, 'B') for n in ('ArtifactID', 'SpellWeight', 'SpellWeightCategory', 'SocketType1', 'SocketType2', 'SocketType3',
                                'SheatheType', 'Material', 'PageMaterialID', 'Bonding', 'DamageDamageType', 'ContainerSlots',
                                'RequiredPVPMedal')]
          + [('RequiredPVPRank', 'b'), ('RequiredLevel', 'b')]
          + [(n, 'B') for n in ('InventoryType', 'OverallQualityID', 'AmmunitionType')])
FMT = '<' + ''.join(t for _, t in FIELDS)
COLUMNS = ['ID'] + STRINGS + [n for n, _ in FIELDS]


def _header(d):
    H = 136
    sc, = struct.unpack_from('<I', d, H + 64)
    secs, p = [], H + 68
    for _ in range(sc):
        secs.append(struct.unpack_from('<Q8I', d, p))
        p += 40
    return secs


def read_sparse(path):
    d = open(path, 'rb').read()
    _, fo, nr, sts, oe, idl, rel, omc, cc = _header(d)[0]
    ids = struct.unpack_from('<%dI' % (idl // 4), d, oe)
    copies = [struct.unpack_from('<II', d, oe + idl + 8 * i) for i in range(cc)]
    omo = oe + idl + 8 * cc
    out = {}
    for i, id_ in enumerate(ids):
        o, sz = struct.unpack_from('<IH', d, omo + 6 * i)
        r = d[o:o + sz]
        pos, row = 0, {'ID': id_}
        for name in STRINGS:
            k = r.index(b'\0', pos)
            row[name] = r[pos:k].decode('utf8', 'replace')
            pos = k + 1
        for (name, _), v in zip(FIELDS, struct.unpack_from(FMT, r, pos)):
            row[name] = v
        out[id_] = row
    for new, src in copies:
        if src in out:
            out[new] = dict(out[src], ID=new)
    return out


def read_randproppoints(path):
    """{ilvl: {'Epic': [5 floats], 'Superior': [...], 'Good': [...]}} (the F columns, which the server uses)."""
    import os, sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vmangos_loot'))
    import wdc5
    rows, _ = wdc5.read(path)
    f = lambda xs: [struct.unpack('<f', struct.pack('<I', x))[0] for x in xs]
    return {k: {'Epic': f(v[4]), 'Superior': f(v[5]), 'Good': f(v[6])} for k, v in rows.items()}


def read_armor_shield(path):
    """{item level: [7 floats]} from ItemArmorShield.db2: the shield armor per quality (index 0 poor .. 6 heirloom)."""
    import os, sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vmangos_loot'))
    import wdc5
    rows, _ = wdc5.read(path)
    # the reader packs the Quality float array into one integer, little endian; the second field is ItemLevel
    return {level: list(struct.unpack('<7f', packed.to_bytes(28, 'little'))) for packed, level in rows.values()}


def read_spell_auras(path):
    """{spell id: {aura type}} of the apply aura effects (Effect 6) in SpellEffect.db2."""
    import os, sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vmangos_loot'))
    import wdc5
    rows, _ = wdc5.read(path)
    out = {}
    # field order of SpellEffectLoadInfo without the ID: EffectAura first, Effect fourth, SpellID last
    for r in rows.values():
        if r[3] == 6:
            out.setdefault(r[-1], set()).add(r[0])
    return out


def read_item_effects(effect_path, x_path):
    """{item id: [ItemEffect dict]} from ItemEffect.db2 and ItemXItemEffect.db2 (the ItemXItemEffect parent field is the item).

    The values come back unsigned; SpellCategoryID reads with bit 16 set (68001 for category 2465), so it is masked to 16 bits."""
    import os, sys
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'vmangos_loot'))
    import wdc5
    s16 = lambda v: v - (1 << 16) if v >= 1 << 15 else v
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    effects, _ = wdc5.read(effect_path)
    links, _ = wdc5.read(x_path)
    out = {}
    for _, (effect_id, item_id) in links.items():
        e = effects.get(effect_id)
        if e is None or item_id is None:
            continue
        out.setdefault(item_id, []).append({
            'ID': effect_id, 'LegacySlotIndex': e[0], 'TriggerType': e[1], 'Charges': s16(e[2]), 'CoolDownMSec': s32(e[3]),
            'CategoryCoolDownMSec': s32(e[4]), 'SpellCategoryID': e[5] & 0xFFFF, 'SpellID': s32(e[6])})
    return out
