import struct,subprocess
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
B=0x08000000
d=bytearray(open(S+'work/wallbox.bin','rb').read())
sy=dict((l.split()[2],int(l.split()[0],16)) for l in subprocess.check_output(['arm-none-eabi-nm',S+'p3/wb3.elf']).decode().splitlines())
def bl(src,dst,cond_b=False):
    off=dst-(src+4); s=1 if off<0 else 0; off&=(1<<25)-1
    i1=(off>>23)&1; i2=(off>>22)&1; imm10=(off>>12)&0x3ff; imm11=(off>>1)&0x7ff
    j1=(1-i1)^s; j2=(1-i2)^s
    return struct.pack('<HH',0xF000|(s<<10)|imm10,0xD000|(j1<<13)|(j2<<11)|imm11)
def b16(src,dst):
    off=(dst-(src+4))//2; assert -1024<=off<1024; return struct.pack('<H',0xE000|(off&0x7ff))
def put(addr,data,expect):
    o=addr-B; assert bytes(d[o:o+len(expect)])==expect,(hex(addr),d[o:o+len(expect)].hex(),expect.hex())
    assert len(data)==len(expect); d[o:o+len(data)]=data
H=lambda *w: struct.pack('<'+'H'*len(w),*w)
assert bl(0x08003e32,0x08002ea8)==bytes(d[0x3e32:0x3e36])
# lunghezza frame stato tipo 1: 15 -> 19 byte
put(0x080012ec,H(0x2313),H(0x230f))
put(0x080015f4,H(0x2b13),H(0x2b0f))
# parser + UID slot 2
put(0x08003866,bl(0x08003866,sy['TRP']),bl(0x08003866,0x080015d4))
# flag "scame" (blocca contattore) sempre 0
put(0x08003a9a,H(0x2300),H(0x2301))
# pagina tempi: AUTO = bit0, SCHUKO1 = bit1 (indipendenti)
put(0x08003bc4,H(0x2301,0xbf00),bytes.fromhex('83f00103'))
put(0x08003be4,H(0x7b7b),H(0x7b3b))
put(0x08003c22,H(0x8a78)+bl(0x08003c24,sy['TR1']),bytes(d[0x3c22:0x3c24])+bl(0x08003c24,0x08002560))
put(0x08003c84,H(0x2301,0xbf00),bytes.fromhex('83f00103'))
put(0x08003ca6,bl(0x08003ca6,sy['TRU'])+b16(0x08003caa,0x08003cc4),bytes(d[0x3ca6:0x3cac]))
# frame tipo 2: pagina conferma per tessere Schuko
put(0x08003e32,bl(0x08003e32,sy['TR2']),bl(0x08003e32,0x08002ea8))
# ripresa dopo blackout: UID popup e stop slot 2
put(0x08003922,bl(0x08003922,sy['TRE']),bytes(d[0x3922:0x3926]))
put(0x080035f0,bl(0x080035f0,sy['TRN']),bl(0x080035f0,0x0800128c))
# popup minuti esauriti SCHUKO 1: usa UID SCHUKO 1 invece di UID AUTO
H2=lambda *w: struct.pack('<'+'H'*len(w),*w)
old=bytes(d[0x3d6e-0:0x3d6e])  # placeholder
o=0x08003d6e-B
assert bytes(d[o:o+20])==bytes.fromhex('97f84c30002b05d007f10c030a331846fff7')+bytes(d[o+18:o+20]), bytes(d[o:o+20]).hex()
cbz=0xb100|(((0x08003d82-(0x08003d72+4))//2)<<3)|3   # cbz r3, 0x08003d82
new=bl(0x08003d6e,sy['TRV'])+H2(cbz,0x4618)+bl(0x08003d76,0x0800322c)+b16(0x08003d7a,0x08003d82)+H2(0xbf00)
put(0x08003d6e,new,bytes(d[o:o+16]))
# stringhe
o=0x080092dc-B; assert d[o:o+17]==b'Ricarica vettura\0'; d[o:o+17]=b'Ricarica\0'+bytes(8)
o=d.find(b'stat.version.txt="5.1"'); assert o>0; d[o+18:o+21]=b'5.4'
code=open(S+'p3/wb3.bin','rb').read()
d+=b'\xff'*(0xA000-len(d))+code
open(S+'p3/wallbox3.bin','wb').write(d); print('ok',hex(B+len(d)))
