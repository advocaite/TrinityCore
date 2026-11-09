# id -> candidate names scraped from the VMaNGOS scripts (SPELL_X = id, // comment) and Forever's Classic scripts. A hint for authoring only.
import re, os, collections, sys
ROOTS = ['/path/to/vmangos/core/src/scripts', '/path/to/wow-foverver/src/server/scripts/Custom/Classic']
pat = re.compile(r'\b((?:SPELL|AURA)_[A-Z0-9_]+)\s*=\s*(\d+)\s*,?\s*(?://\s*(.*))?')
def scan():
    d = collections.defaultdict(set)
    for root in ROOTS:
        for dp, _, fs in os.walk(root):
            for f in fs:
                if not f.endswith(('.cpp', '.h')): continue
                for line in open(os.path.join(dp, f), encoding='utf-8', errors='replace'):
                    m = pat.search(line)
                    if m:
                        d[int(m.group(2))].add((m.group(1) + ((' // ' + m.group(3).strip()[:60]) if m.group(3) else '')))
    return d
if __name__ == '__main__':
    d = scan()
    for a in sys.argv[1:]:
        print(a, sorted(d.get(int(a), []))[:4])
