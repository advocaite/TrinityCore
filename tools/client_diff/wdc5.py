"""Generic WDC5 (DB2) reader for the Classic 1.60 client files.

Field layout and names come from our own server source (src/server/game/DataStores/DB2Metadata.h and DB2LoadInfo.h),
so a table reads the same way the worldserver reads it. Encrypted sections (tact key set, data zeroed) are skipped and
counted, so the caller can report them. Sections the extractor decrypted carry the TRINITY marker and are read like plain ones.

    meta = Schemas('/path/to/repo')
    rows, info = read_db2('ItemSparse.db2', meta)   # rows: {id: {field: value}}
"""
import re
import struct

# mapextractor writes this in place of the key name of a section it could decrypt (DUMMY_KNOWN_TACT_ID, DB2FileLoader.h)
DECRYPTED_TACT_ID = 0x5452494E49545900

# tables the server reads without a LoadInfo of their own
FALLBACK_NAMES = {'Spell': ['ID', 'NameSubtext', 'Description', 'AuraDescription']}
FT_SIZE = {'FT_BYTE': 1, 'FT_SHORT': 2, 'FT_INT': 4, 'FT_LONG': 8, 'FT_FLOAT': 4, 'FT_STRING': 4,
           'FT_STRING_NOT_LOCALIZED': 4}


class Schemas:
    def __init__(self, repo):
        base = repo + '/src/server/game/DataStores/'
        self.meta = {}
        txt = open(base + 'DB2Metadata.h', encoding='utf8').read()
        for m in re.finditer(r'struct (\w+)Meta\s*\{(.*?)\n\};', txt, re.S):
            name, body = m.group(1), m.group(2)
            fields = [(t, int(a), s == 'true') for t, a, s in
                      re.findall(r'\.Type = (FT_\w+),\s*\.ArraySize =\s*(\d+), \.IsSigned =\s*(\w+)', body)]
            g = lambda k: int(re.search(r'\.%s\s*=\s*(-?\w+)' % k, body).group(1), 0)
            self.meta[name] = dict(fields=fields, index=g('IndexField'), parent=g('ParentIndexField'),
                                   count=g('FieldCount'), file_count=g('FileFieldCount'), layout=g('LayoutHash'))
        self.names = {}
        txt = open(base + 'DB2LoadInfo.h', encoding='utf8').read()
        for m in re.finditer(r'struct (\w+)LoadInfo\s*\{(.*?)\n\};', txt, re.S):
            self.names[m.group(1)] = re.findall(r'\.Name = "(\w+)"', m.group(2))


def _flat_names(sch, table):
    """Names for the meta fields, one list per meta field (arrays get one name per element)."""
    meta = sch.meta[table]
    names = list(sch.names.get(table) or FALLBACK_NAMES.get(table, []))
    out = []
    if meta['index'] == -1:
        idname = names.pop(0) if names else 'ID'
    else:
        idname = None
    for i, (t, n, s) in enumerate(meta['fields']):
        if len(names) >= n:
            out.append(names[:n])
            names = names[n:]
        else:
            out.append(['f%d_%d' % (i, k) for k in range(n)] if n > 1 else ['f%d' % i])
    if idname is None:
        idname = out[meta['index']][0]
    return idname, out


def read_db2(path, sch, table=None):
    d = open(path, 'rb').read()
    if table is None:
        table = re.sub(r'(-.*)?\.db2$', '', path.replace('\\', '/').split('/')[-1])
    meta = sch.meta[table]
    if d[:4] != b'WDC5':
        raise ValueError('%s: not WDC5' % path)
    H = 136
    (rc, fc, rs, sts, th, lh, minid, maxid, loc, flags, idxf, tfc, bpo, lcc, fsis, cds, pds, sc) = \
        struct.unpack_from('<4I2I3I2H7I', d, H)
    info = dict(table=table, records=rc, layout='%08X' % lh, layout_src='%08X' % meta['layout'],
                sections=sc, encrypted_sections=0, encrypted_records=0)
    p = H + 68
    secs = []
    for _ in range(sc):
        secs.append(struct.unpack_from('<Q8I', d, p))
        p += 40
    p += 4 * fc
    fsi = [struct.unpack_from('<HHIIIII', d, p + 24 * i) for i in range(fsis // 24)]
    p += fsis
    pallet = {}
    o = p
    for i, f in enumerate(fsi):
        if f[3] in (3, 4):
            pallet[i] = d[o:o + f[2]]
            o += f[2]
    p += pds
    common = {}
    o = p
    for i, f in enumerate(fsi):
        if f[3] == 2:
            m = {}
            for k in range(f[2] // 8):
                a, b = struct.unpack_from('<II', d, o + 8 * k)
                m[a] = b
            common[i] = m
            o += f[2]
    idname, names = _flat_names(sch, table)
    mf = meta['fields']
    nfile = meta['file_count']
    sparse = bool(flags & 1)
    # global string table: concatenated per-section string blocks
    total_rec = sum(s[2] for s in secs) * rs if not sparse else 0
    strblk = bytearray()
    for s in secs:
        if not sparse:
            strblk += d[s[1] + s[2] * rs:s[1] + s[2] * rs + s[3]]
    strblk = bytes(strblk)
    rows = {}
    recbefore = 0

    def tofloat(v):
        return round(struct.unpack('<f', struct.pack('<I', v & 0xFFFFFFFF))[0], 6)

    def sx(v, bits):
        return v - (1 << bits) if v >> (bits - 1) & 1 else v

    for sec in secs:
        tact, fo, nr, ssz, ore, idl, rel, omc, cc = sec
        if tact and tact != DECRYPTED_TACT_ID:
            # encrypted section: data is zero unless the key is known
            info['encrypted_sections'] += 1
            info['encrypted_records'] += nr
            if not sparse:
                recbefore += nr
            continue
        if sparse:
            ids_o = ore
        else:
            ids_o = fo + nr * rs + ssz
        ids = list(struct.unpack_from('<%dI' % (idl // 4), d, ids_o)) if idl else None
        cp_o = ids_o + idl
        copies = [struct.unpack_from('<II', d, cp_o + 8 * i) for i in range(cc)]
        om_o = cp_o + 8 * cc
        offmap = [struct.unpack_from('<IH', d, om_o + 6 * i) for i in range(omc)] if sparse else None
        rel_o = om_o + 6 * omc
        relm = {}
        if rel:
            n, mn, mx = struct.unpack_from('<3I', d, rel_o)
            for k in range(n):
                fk, ri = struct.unpack_from('<II', d, rel_o + 12 + 8 * k)
                relm[ri] = fk
        nrec = omc if sparse else nr
        for r in range(nrec):
            vals = []
            if sparse:
                ro, sz = offmap[r]
                if sz == 0:
                    continue
                q = ro
                for i in range(nfile):
                    t, n, s = mf[i]
                    arr = []
                    for _ in range(n):
                        if t.startswith('FT_STRING'):
                            k = d.index(b'\0', q)
                            arr.append(d[q:k].decode('utf8', 'replace'))
                            q = k + 1
                        else:
                            sz2 = FT_SIZE[t]
                            v = int.from_bytes(d[q:q + sz2], 'little', signed=s and t != 'FT_FLOAT')
                            arr.append(tofloat(v) if t == 'FT_FLOAT' else v)
                            q += sz2
                    vals.append(arr)
            else:
                ro = fo + r * rs
                bits = int.from_bytes(d[ro:ro + rs] + b'\0' * 8, 'little')
                for i in range(nfile):
                    t, n, s = mf[i]
                    off, size, addl, ctype, a, b, c = fsi[i]
                    arr = []
                    if ctype == 0:
                        esz = size // n
                        for k in range(n):
                            v = (bits >> (off + k * esz)) & ((1 << esz) - 1)
                            if t.startswith('FT_STRING'):
                                if v == 0:
                                    arr.append('')
                                else:
                                    gpos = (recbefore + r) * rs + off // 8 + k * (esz // 8) + v - total_rec
                                    if 0 <= gpos < len(strblk):
                                        e = strblk.index(b'\0', gpos)
                                        arr.append(strblk[gpos:e].decode('utf8', 'replace'))
                                    else:
                                        arr.append('<bad string>')
                            elif t == 'FT_FLOAT':
                                arr.append(tofloat(v))
                            else:
                                arr.append(sx(v, esz) if s else v)
                    elif ctype in (1, 5):
                        v = (bits >> off) & ((1 << b) - 1)
                        if ctype == 5:
                            v = sx(v, b)
                        arr.append(tofloat(v) if t == 'FT_FLOAT' else v)
                    elif ctype == 2:
                        arr.append(None)  # filled after the id is known
                    elif ctype in (3, 4):
                        idx = (bits >> off) & ((1 << b) - 1)
                        cnt = c if ctype == 4 else 1
                        pv = pallet[i]
                        for k in range(cnt):
                            v, = struct.unpack_from('<I', pv, (idx * cnt + k) * 4)
                            if t == 'FT_FLOAT':
                                arr.append(tofloat(v))
                            elif s and FT_SIZE.get(t, 4) <= 4:
                                arr.append(sx(v, 32))
                            else:
                                arr.append(v)
                    vals.append(arr)
            # integers keep the width and sign of their meta type, as the server's int8/int16 fields do
            for i in range(nfile):
                t, n, sg = mf[i]
                if t in ('FT_BYTE', 'FT_SHORT', 'FT_INT') and fsi[i][3] != 2:
                    bw = FT_SIZE[t] * 8
                    if sg:
                        vals[i] = [sx(v & ((1 << bw) - 1), bw) if isinstance(v, int) else v for v in vals[i]]
                    else:
                        vals[i] = [v & ((1 << bw) - 1) if isinstance(v, int) else v for v in vals[i]]
            if meta['index'] == -1:
                rid = ids[r] if ids else minid + r
            else:
                rid = vals[meta['index']][0]
            if not sparse:
                for i in range(nfile):
                    if fsi[i][3] == 2:
                        t = mf[i][0]
                        v = common[i].get(rid, fsi[i][4])
                        vals[i] = [tofloat(v) if t == 'FT_FLOAT' else (sx(v & ((1 << FT_SIZE[t] * 8) - 1), FT_SIZE[t] * 8) if mf[i][2] and t in ('FT_BYTE', 'FT_SHORT', 'FT_INT') else v)]
            if meta['count'] > nfile:
                vals.append([relm.get(r, 0)])
            row = {idname: rid}
            for nm, arr in zip(names, vals):
                for k, v in zip(nm, arr):
                    row[k] = v
            rows[rid] = row
        for new, src in copies:
            if src in rows:
                rr = dict(rows[src])
                rr[idname] = new
                rows[new] = rr
        if not sparse:
            recbefore += nr
    info['decoded'] = len(rows)
    info['id_field'] = idname
    return rows, info
