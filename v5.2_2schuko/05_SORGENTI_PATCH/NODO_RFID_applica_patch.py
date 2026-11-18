import subprocess,os
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
P=S+'patch/'
d=bytearray(open(S+'work/rfid.bin','rb').read())
SYMS='DW=0x412;SENDERR=0x1250;L2728=0x2728;L2D78=0x2d78;L282A=0x282a;L37CE=0x37ce;L35C0=0x35c0;L3574=0x3574;S1=0x3734;S2=0x373c;'
def asm(addr,src):
    open(P+'t.s','w').write('.text\n'+src+'\n')
    open(P+'t.ld','w').write(SYMS.replace('=',' = ').replace(';',';\n'))
    subprocess.check_call(['avr-as','-mmcu=atmega328p',P+'t.s','-o',P+'t.o'])
    subprocess.check_call(['avr-ld','-mavr5','-Ttext=%#x'%addr,'-T',P+'t.ld',P+'t.o','-o',P+'t.elf'])
    subprocess.check_call(['avr-objcopy','-O','binary',P+'t.elf',P+'t.bin'])
    return open(P+'t.bin','rb').read()
def put(addr,data,expect_hex):
    exp=bytes.fromhex(expect_hex.replace(' ',''))
    assert bytes(d[addr:addr+len(exp)])==exp,(hex(addr),d[addr:addr+len(exp)].hex(),exp.hex())
    assert len(data)<=len(exp) or expect_hex=='' 
    d[addr:addr+len(data)]=data
# R1 accettazione tessera (modalita' 1)
r1=asm(0x3596,'cpi r17,1\nbrne .+2\nrjmp L37CE\ncpi r17,3\nbreq L35C0\nldi r24,8\nrjmp L3574')
put(0x3596,r1,'9f ef 91 0f 92 30 08 f4 17 c1 13 30 09 f0')
# R2 conferma: slot scelto da 0x1F6 per qualunque modalita'
put(0x2d88,bytes(2),'39 f4')
# R3 presa scelta occupata -> S1
put(0x2db4,asm(0x2db4,'rjmp S1'),'e1 cf')
# R4 bit1 stato = slot A tipo 3 (SCHUKO 1)
put(0x2804,asm(0x2804,'cpi r19,3'),'32 30')
# R5 campo b = minuti slot A
put(0x2826,asm(0x2826,'rjmp S2\nnop'),'80 91 a7 02')
# R6 blocco uscite + stub
blk=open(P+'rf_block.bin','rb').read()
assert 0x36e0+len(blk)<=0x3798
old=bytes(d[0x36e0:0x3798]); assert old[:4]==bytes.fromhex('6091a102') and d[0x3798:0x379c]==bytes.fromhex('2230 09f0'.replace(' ',''))
d[0x36e0:0x3798]=blk+bytes(0x3798-0x36e0-len(blk))
open(P+'rfid_new.bin','wb').write(d)
print('ok',len(d))
