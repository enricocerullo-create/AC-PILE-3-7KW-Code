def read_hex(path):
    mem={}; base=0; start=None
    for line in open(path):
        line=line.strip()
        if not line.startswith(':'): continue
        b=bytes.fromhex(line[1:]); n=b[0]; a=(b[1]<<8)|b[2]; t=b[3]; data=b[4:4+n]
        assert sum(b)&0xff==0, 'checksum '+line
        if t==0:
            for i,x in enumerate(data): mem[base+a+i]=x
        elif t==4: base=((data[0]<<8)|data[1])<<16
        elif t==2: base=((data[0]<<8)|data[1])<<4
        elif t==5: start=data
        elif t==1: break
    return mem,start
def rec(t,a,data):
    b=bytes([len(data),(a>>8)&0xff,a&0xff,t])+bytes(data)
    return ':'+(b+bytes([(-sum(b))&0xff])).hex().upper()
def write_hex(path,mem,start=None,ext=True):
    out=[]; cur_hi=None
    addrs=sorted(mem); i=0
    while i<len(addrs):
        a=addrs[i]; chunk=[mem[a]]; j=i+1
        while j<len(addrs) and addrs[j]==a+len(chunk) and len(chunk)<16 and ((a+len(chunk))&0xffff)!=0 and ((a & ~0xf)==((a+len(chunk)) & ~0xf)):
            chunk.append(mem[addrs[j]]); j+=1
        hi=a>>16
        if ext and hi!=cur_hi: out.append(rec(4,0,[hi>>8,hi&0xff])); cur_hi=hi
        out.append(rec(0,a&0xffff,chunk)); i=j
    if start: out.append(rec(5,0,start))
    out.append(':00000001FF')
    open(path,'w',newline='\r\n').write('\n'.join(out)+'\n')
