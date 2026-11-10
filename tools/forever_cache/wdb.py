#!/usr/bin/env python3
"""Read the WoW Forever client's own cache files (Cache/WDB/<locale>/*.wdb).

The client keeps every quest, creature and game object answer it got from a server in these files, so a player's cache holds
the full quest texts, objectives and rewards, creature names/models/types and object templates of everything they met.
Each record is the server answer as sent (the layouts below follow our own writers in src/server/game/Server/Packets/
QuestPackets.cpp and QueryPackets.cpp, which match client 1.60.1.70862).

File: 24-byte header (magic 'WQST'/'WMOB'/'WGOB' stored reversed, build, locale reversed, record size hint, record version,
unknown), then records: uint32 id, uint32 size, size bytes; a 0/0 pair or the end of the file ends the list.

    python3 -I tools/forever_cache/wdb.py <folder or .wdb files...> --out cache.json

Only quest, creature and game object caches are read; other .wdb files are listed and skipped.
"""
import argparse
import json
import os
import struct
import sys


class Reader:
    def __init__(self, data):
        self.d, self.p, self.bitpos, self.bitval = data, 0, 8, 0

    def take(self, n):
        if self.p + n > len(self.d):
            raise EOFError('record ends early at %d (+%d of %d)' % (self.p, n, len(self.d)))
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def i32(self): self.flush(); return struct.unpack('<i', self.take(4))[0]
    def u32(self): self.flush(); return struct.unpack('<I', self.take(4))[0]
    def i64(self): self.flush(); return struct.unpack('<q', self.take(8))[0]
    def i8(self): self.flush(); return struct.unpack('<b', self.take(1))[0]
    def u8(self): self.flush(); return self.take(1)[0]
    def f32(self): self.flush(); return round(struct.unpack('<f', self.take(4))[0], 6)

    def bits(self, n):
        v = 0
        for _ in range(n):
            if self.bitpos == 8:
                self.bitval, self.bitpos = self.take(1)[0], 0
            v = (v << 1) | ((self.bitval >> (7 - self.bitpos)) & 1)
            self.bitpos += 1
        return v

    def flush(self):
        self.bitpos = 8

    def text(self, n):
        self.flush()
        return self.take(n).decode('utf-8', 'replace')

    def cstring(self):
        self.flush()
        e = self.d.index(b'\0', self.p)
        s = self.d[self.p:e].decode('utf-8', 'replace')
        self.p = e + 1
        return s


def quest(r):
    q = {}
    for k in ('ID', 'LevelType', 'QuestLevel', 'QuestPackageID', 'MinLevel', 'ContentTuningID', 'QuestSortID', 'QuestInfoID',
              'SuggestedGroupNum', 'RewardNextQuest', 'RewardXPDifficulty'):
        q[k] = r.i32()
    q['RewardXPMultiplier'] = r.f32()
    q['RewardMoney'] = r.i32()
    q['RewardMoneyDifficulty'] = r.i32()
    q['RewardMoneyMultiplier'] = r.f32()
    q['RewardBonusMoney'] = r.i32()
    n_display = r.u32()
    q['RewardSpell'] = r.i32()
    q['RewardHonor'] = r.i32()
    q['RewardKillHonor'] = r.f32()
    q['RewardFavor'] = r.i32()
    q['RewardArtifactXPDifficulty'] = r.i32()
    q['RewardArtifactXPMultiplier'] = r.f32()
    q['RewardArtifactCategoryID'] = r.i32()
    q['StartItem'] = r.i32()
    q['Flags'], q['FlagsEx'], q['FlagsEx2'], q['FlagsEx3'] = r.u32(), r.u32(), r.u32(), r.u32()
    q['RewardItems'] = [dict(ItemID=r.i32(), Amount=r.i32(), ItemDrop=r.i32(), ItemDropQuantity=r.i32()) for _ in range(4)]
    q['RewardChoiceItems'] = [dict(ItemID=r.i32(), Quantity=r.i32(), DisplayID=r.i32()) for _ in range(6)]
    q['POIContinent'], q['POIx'], q['POIy'], q['POIPriority'] = r.i32(), r.f32(), r.f32(), r.i32()
    for k in ('RewardTitle', 'RewardArenaPoints', 'RewardSkillLineID', 'RewardNumSkillUps', 'PortraitGiver', 'PortraitGiverMount',
              'PortraitGiverModelSceneID', 'PortraitTurnIn'):
        q[k] = r.i32()
    q['RewardFactions'] = [dict(FactionID=r.i32(), Value=r.i32(), Override=r.i32(), CapIn=r.i32()) for _ in range(5)]
    q['RewardFactionFlags'] = r.i32()
    q['RewardCurrencies'] = [dict(CurrencyID=r.i32(), Quantity=r.i32()) for _ in range(4)]
    q['AcceptedSoundKitID'], q['CompleteSoundKitID'], q['AreaGroupID'] = r.i32(), r.i32(), r.i32()
    q['TimeAllowed'] = r.i64()
    n_obj = r.u32()
    lo, hi = r.u32(), r.u32()
    q['AllowableRaces'] = lo | (hi << 32)
    n_tp, n_ntp = r.u32(), r.u32()
    q['Expansion'], q['ManagedWorldStateID'], q['QuestSessionBonus'], q['QuestGiverCreatureID'] = r.i32(), r.i32(), r.i32(), r.i32()
    n_cdesc, n_clog, n_room, n_decor = r.u32(), r.u32(), r.u32(), r.u32()
    q['RewardDisplaySpell'] = [dict(SpellID=r.i32(), PlayerConditionID=r.i32(), Type=r.i32()) for _ in range(n_display)]
    objs = []
    for _ in range(n_obj):
        o = dict(ID=r.u32(), Type=r.i32(), StorageIndex=r.i8(), ObjectID=r.i32(), Amount=r.i32(), ConditionalAmount=r.i32(),
                 Flags=r.u32(), Flags2=r.u32(), ProgressBarWeight=r.f32())
        n_vis = r.i32()
        o['ParentObjectiveID'] = r.i32()
        o['VisualEffects'] = [r.i32() for _ in range(n_vis)]
        dlen = r.bits(8)
        o['Visible'] = r.bits(1)
        o['Description'] = r.text(dlen)
        objs.append(o)
    q['Objectives'] = objs
    q['TreasurePickerID'] = [r.i32() for _ in range(n_tp)]
    q['NonDisplayableTreasurePickerIDs'] = [r.i32() for _ in range(n_ntp)]

    def cond_texts(n):
        out = []
        for _ in range(n):
            pc, giver = r.i32(), r.i32()
            ln = r.bits(12)
            out.append(dict(PlayerConditionID=pc, QuestGiverCreatureID=giver, Text=r.text(ln)))
        return out
    q['ConditionalQuestDescription'] = cond_texts(n_cdesc)
    q['ConditionalQuestCompletionLog'] = cond_texts(n_clog)
    q['RewardHouseRoomIDs'] = [r.i32() for _ in range(n_room)]
    q['RewardHouseDecorIDs'] = [r.i32() for _ in range(n_decor)]
    names = ('LogTitle', 'LogDescription', 'QuestDescription', 'AreaDescription', 'PortraitGiverText', 'PortraitGiverName',
             'PortraitTurnInText', 'PortraitTurnInName', 'QuestCompletionLog')
    lens = [r.bits(n) for n in (9, 12, 12, 9, 10, 8, 10, 8, 11)]
    q['ResetByScheduler'], q['ReadyForTranslation'] = r.bits(1), r.bits(1)
    for k, n in zip(names, lens):
        q[k] = r.text(n)
    return q


def creature(r):
    c = {}
    lt, lta, lc = r.bits(11), r.bits(11), r.bits(6)
    c['Leader'] = r.bits(1)
    ln = [(r.bits(11), r.bits(11)) for _ in range(4)]
    names, alts = [], []
    for a, b in ln:
        names.append(r.text(a).rstrip('\0'))
        alts.append(r.text(b).rstrip('\0'))
    c['Name'], c['NameAlt'] = names, alts
    c['Flags'] = [r.u32() for _ in range(3)]
    c['CreatureType'] = r.u8()
    c['CreatureFamily'] = r.i32()
    c['Classification'] = r.i8()
    c['ProxyCreatureID'] = [r.u32() for _ in range(2)]
    n_disp = r.u32()
    c['TotalProbability'] = r.f32()
    c['Displays'] = [dict(CreatureDisplayID=r.u32(), Scale=r.f32(), Probability=r.f32()) for _ in range(n_disp)]
    c['HpMulti'], c['EnergyMulti'] = r.f32(), r.f32()
    n_qi, n_qc = r.u32(), r.u32()
    for k in ('CreatureMovementInfoID', 'HealthScalingExpansion', 'RequiredExpansion', 'VignetteID', 'Class', 'CreatureDifficultyID',
              'WidgetSetID', 'WidgetSetUnitConditionID'):
        c[k] = r.i32()
    c['Title'] = r.text(lt).rstrip('\0')
    c['TitleAlt'] = r.text(lta).rstrip('\0')
    c['CursorName'] = r.text(lc).rstrip('\0')
    c['QuestItems'] = [r.i32() for _ in range(n_qi)]
    c['QuestCurrencies'] = [r.i32() for _ in range(n_qc)]
    return c


def gameobject(r):
    g = dict(Type=r.i32(), DisplayID=r.i32())
    g['Name'] = [r.cstring() for _ in range(4)]
    g['IconName'], g['CastBarCaption'], g['UnkString'] = r.cstring(), r.cstring(), r.cstring()
    g['Data'] = [r.i32() for _ in range(35)]
    g['Size'] = r.f32()
    g['QuestItems'] = [r.i32() for _ in range(r.u8())]
    g['ContentTuningId'], g['RequiredLevel'] = r.i32(), r.i32()
    return g


KINDS = {b'TSQW': ('quests', quest), b'BOMW': ('creatures', creature), b'BOGW': ('gameobjects', gameobject)}


def read_file(path, out, problems):
    d = open(path, 'rb').read()
    if len(d) < 24:
        return
    magic, build, loc = d[:4], struct.unpack_from('<I', d, 4)[0], d[8:12][::-1].decode('ascii', 'replace')
    if magic not in KINDS:
        problems.append('%s: %s cache not read' % (path, magic[::-1].decode('ascii', 'replace')))
        return
    kind, fn = KINDS[magic]
    p = 24
    while p + 8 <= len(d):
        rid, size = struct.unpack_from('<II', d, p)
        if rid == 0 and size == 0:
            break
        rec = d[p + 8:p + 8 + size]
        p += 8 + size
        try:
            r = Reader(rec)
            v = fn(r)
            if r.p != len(rec):
                problems.append('%s %s %d: %d bytes left over' % (os.path.basename(path), kind, rid, len(rec) - r.p))
        except (EOFError, ValueError, struct.error) as e:
            problems.append('%s %s %d: %s' % (os.path.basename(path), kind, rid, e))
            continue
        v['_build'], v['_locale'], v['_file'] = build, loc, path
        out[kind].setdefault(loc, {})[str(rid)] = v


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('paths', nargs='+', help='WDB folders (searched recursively) or .wdb files')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    files = []
    for p in a.paths:
        if os.path.isdir(p):
            for root, _, names in os.walk(p):
                files += [os.path.join(root, n) for n in sorted(names) if n.lower().endswith('.wdb')]
        else:
            files.append(p)
    out, problems = {'quests': {}, 'creatures': {}, 'gameobjects': {}}, []
    for f in files:
        read_file(f, out, problems)
    json.dump(out, open(a.out, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    for kind in out:
        for loc, recs in out[kind].items():
            print('%s %s: %d' % (kind, loc, len(recs)))
    for pr in problems:
        print('PROBLEM', pr, file=sys.stderr)


if __name__ == '__main__':
    main()
