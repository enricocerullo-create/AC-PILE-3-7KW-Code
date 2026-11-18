import struct,subprocess,sys
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
B=0x08000000
d=bytearray(open(S+'work/wallbox.bin','rb').read())
def bl(src,dst):
    off=dst-(src+4); assert -(1<<24)<=off<(1<<24) and off%2==0
    s=1 if off<0 else 0; off&=(1<<25)-1
    i1=(off>>23)&1; i2=(off>>22)&1; imm10=(off>>12)&0x3ff; imm11=(off>>1)&0x7ff
    j1=(1-i1)^s; j2=(1-i2)^s
    h1=0xF000|(s<<10)|imm10; h2=0xD000|(j1<<13)|(j2<<11)|imm11
    return struct.pack('<HH',h1,h2)
def put(addr,data,expect=None):
    o=addr-B
    if expect is not None: assert bytes(d[o:o+len(expect)])==expect,(hex(addr),d[o:o+len(expect)].hex(),expect.hex())
    d[o:o+len(data)]=data
# sanity: re-encode an existing BL to validate encoder (0x08003e32 -> 0x08002ea8)
assert bl(0x08003e32,0x08002ea8)==bytes(d[0x3e32:0x3e36]), 'bl encoder'
import subprocess
sy=dict(l.split()[::-2] for l in subprocess.check_output(['arm-none-eabi-nm',S+'patch/wb_tramp.elf']).decode().splitlines())
TR1,TR2,TR6=[int(sy[k],16) for k in ('TR1','TR2','TR6')]
print(hex(TR1),hex(TR2),hex(TR6))
# 1) tempi.eneScame -> minuti
put(0x08003c22, struct.pack('<H',0x8A78)+bl(0x08003c24,TR1), expect=bytes(d[0x3c22:0x3c24])+bl(0x08003c24,0x08002560))
# 2) frame tipo 2: pagina conferma per tessere Schuko
put(0x08003e32, bl(0x08003e32,TR2), expect=bl(0x08003e32,0x08002ea8))
# 4) EVSE autorizzato solo per AUTO
put(0x08003a18, bl(0x08003a18,TR6)+struct.pack('<H',0xBF00), expect=bytes.fromhex('3b7b87f84d30'))
# 5) stringhe
o=0x080092dc-B; assert d[o:o+17]==b'Ricarica vettura\0'; put(0x080092dc,b'Ricarica\0'+b'\0'*8)
o=d.find(b'stat.version.txt="5.1"'); assert o>0; d[o+18:o+21]=b'5.2'
# 6) codice aggiunto
code=open(S+'patch/wb_tramp.bin','rb').read()
end=len(d); pad=(0xA000)-end; assert pad>0
d+=b'\xff'*pad+code
open(S+'patch/wallbox_new.bin','wb').write(d)
print('ok',hex(B+len(d)))
