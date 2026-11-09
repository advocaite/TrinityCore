import struct, sys
def read(path):
    d=open(path,'rb').read()
    H=136
    (rc,fc,rs,sts,th,lh,minid,maxid,loc,flags,idx,tfc,bpo,lcc,fsis,cds,pds,sc)=struct.unpack_from('<4I2I3I2H7I',d,H)
    p=H+68
    secs=[]
    for _ in range(sc):
        secs.append(struct.unpack_from('<Q8I',d,p)); p+=40
    p+=4*fc
    fsi=[]
    for i in range(fsis//24):
        fsi.append(struct.unpack_from('<HHIIIII',d,p)); p+=24
    pallet_base=p; p+=pds
    # pallet offsets per field
    poff=[];o=0
    for f in fsi:
        poff.append(o)
        if f[3] in (3,4): o+=f[2]
    common_base=p
    common={};o=common_base
    for i,f in enumerate(fsi):
        if f[3]==2:
            m={}
            for k in range(f[2]//8):
                a,b=struct.unpack_from('<II',d,o+8*k); m[a]=b
            common[i]=m; o+=f[2]
    rows={}
    for sec in secs:
        _,fo,nr,ssz,ore,idl,rel,omc,cc=sec
        if flags&1: raise SystemExit('sparse not handled')
        recbase=fo
        ids_o=fo+nr*rs+ssz
        ids=list(struct.unpack_from('<%dI'%(idl//4),d,ids_o)) if idl else None
        cp_o=ids_o+idl
        copies=[struct.unpack_from('<II',d,cp_o+8*i) for i in range(cc)]
        ro=cp_o+8*cc+6*omc  # offset map
        relm={}
        if rel:
            n,mn,mx=struct.unpack_from('<3I',d,ro)
            for k in range(n):
                fk,ri=struct.unpack_from('<II',d,ro+12+8*k); relm[ri]=fk
        for r in range(nr):
            ro2=recbase+r*rs
            bits=int.from_bytes(d[ro2:ro2+rs]+b'\0'*8,'little')
            vals=[]
            for i,f in enumerate(fsi):
                off,size,addl,ctype,a,b,c=f
                if ctype==0:
                    v=(bits>>off)&((1<<size)-1)
                    if size in(8,16,32,64):
                        pass
                    vals.append(v)
                elif ctype in(1,5):
                    v=(bits>>off)&((1<<size)-1)
                    if ctype==5 and v>>(size-1): v-=1<<size
                    vals.append(v)
                elif ctype==2:
                    vals.append(None)
                elif ctype==3:
                    pi=(bits>>off)&((1<<size)-1)
                    vals.append(struct.unpack_from('<I',d,pallet_base+poff[i]+pi*4)[0])
                elif ctype==4:
                    pi=(bits>>off)&((1<<size)-1); n=c
                    vals.append(list(struct.unpack_from('<%dI'%n,d,pallet_base+poff[i]+pi*4*n)))
            id_=ids[r] if ids else vals[idx]
            for i,f in enumerate(fsi):
                if f[3]==2: vals[i]=common[i].get(id_,f[4])
            if rel: vals.append(relm.get(r))
            rows[id_]=vals
        for new,src in copies:
            if src in rows: rows[new]=list(rows[src])
    return rows,fsi
if __name__=='__main__':
    rows,fsi=read(sys.argv[1])
    print(len(rows),'rows; fields',[(f[0],f[1],f[3]) for f in fsi])
    for k in list(rows)[:5]: print(k,rows[k])
