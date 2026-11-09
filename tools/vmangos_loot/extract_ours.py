# usage: extract_ours.py <repo>/sql <out.sql>; load ours_schema.sql, then the output, into a database named `ours`
# Pull every statement touching the loot tables (and LootID columns) from our world SQL, in updater (name) order.
import os,re,sys
root=sys.argv[1]  # the repo's sql folder
files=[]
for d in ('updates/world/master','custom/world'):
    for f in os.listdir(os.path.join(root,d)):
        if f.endswith('.sql'): files.append((f,os.path.join(root,d,f)))
files.sort()
T=('creature_loot_template','reference_loot_template','skinning_loot_template','pickpocketing_loot_template','creature_template_difficulty','conditions')
out=open(sys.argv[2],'w')
for name,p in files:
    if name < '2026_09_27': continue
    txt=open(p,encoding='utf-8',errors='replace').read()
    if not any(t in txt for t in T): continue
    out.write(f"-- FILE {name}\n")
    buf=[]
    for line in txt.splitlines(True):
        buf.append(line)
        if line.rstrip().endswith(';'):
            st=''.join(buf); buf=[]
            s=re.sub(r'^\s*(--[^\n]*\n\s*)*','',st)
            m=re.match(r"(DELETE FROM|INSERT INTO|INSERT IGNORE INTO|REPLACE INTO|UPDATE)\s+`?(\w+)`?",s,re.I)
            if m and m.group(2) in T:
                out.write(s+'\n')
print('ok')
