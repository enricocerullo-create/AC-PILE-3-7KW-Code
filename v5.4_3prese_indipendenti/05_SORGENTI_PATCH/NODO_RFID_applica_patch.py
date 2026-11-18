import subprocess,re,struct
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
P=S+'p3/'
d=bytearray(open(S+'work/rfid.bin','rb').read())
sy={}
for l in subprocess.check_output(['avr-nm',P+'rf3.elf']).decode().splitlines():
    v,t,n=l.split(); sy[n]=int(v,16)
sy.update({'T37CE':0x37ce,'T35C0':0x35c0,'T3574':0x3574,'T2ACE':0x2ace})
SY=';'.join(f'{k} = {v:#x}' for k,v in sy.items() if k[0].isupper())+';'
def asm(addr,src):
    open(P+'t.s','w').write('.text\n'+src+'\n'); open(P+'t.ld','w').write(SY.replace(';',';\n'))
    subprocess.check_call(['avr-as','-mmcu=atmega328p',P+'t.s','-o',P+'t.o'])
    subprocess.check_call(['avr-ld','-mavr5','-Ttext=%#x'%addr,'-T',P+'t.ld',P+'t.o','-o',P+'t.elf'])
    subprocess.check_call(['avr-objcopy','-O','binary',P+'t.elf',P+'t.bin'])
    return open(P+'t.bin','rb').read()
def put(addr,src,expect):
    exp=bytes.fromhex(expect.replace(' ',''))
    assert bytes(d[addr:addr+len(exp)])==exp,(hex(addr),d[addr:addr+len(exp)].hex(),exp.hex())
    code=asm(addr,src) if isinstance(src,str) else src
    assert len(code)<=len(exp),(hex(addr),len(code),len(exp))
    d[addr:addr+len(code)]=code
# --- P1 rilocazione variabili 0x2D1..0x2E8 -> 0x3D0..0x3E7 (solo lds/sts)
lst=subprocess.check_output(['avr-objdump','-D','-m','avr5','-b','binary',S+'work/rfid.bin']).decode()
n=0
for l in lst.splitlines():
    m=re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){4})\s*(lds|sts)\s',l)
    if not m: continue
    a=int(m.group(1),16); w2=struct.unpack('<H',d[a+2:a+4])[0]
    if 0x2D1<=w2<=0x2E8:
        d[a+2:a+4]=struct.pack('<H',w2+0xFF); n+=1
assert n==52,n
# --- P0 azzeramento .bss esteso fino a 0x400
put(0x2e8,'ldi r18,0x04','23 e0'); put(0x2f2,'cpi r26,0x00','a2 3c')
# --- P2 accettazione tessere (modalità 1)
put(0x3596,'cpi r17,1\nbrne .+2\nrjmp T37CE\ncpi r17,3\nbreq T35C0\nldi r24,8\nrjmp T3574','9f ef 91 0f 92 30 08 f4 17 c1 13 30 09 f0')
# --- P3 Schuko: rifiuta solo se entrambe occupate
put(0x35c0,'jmp NEWT3','bb 20 09 f4 07 c1')
# --- P4 conferma: scelta slot
put(0x2d82,'jmp CONF','80 91 90 02')
# --- P5 presa scelta occupata
put(0x2db4,'rjmp S1','e1 cf')
# --- P6 ciclo crediti su 3 slot
put(0x2e28,bytes(2),'ff 24'); put(0x2e82,'ldi r31,2','f1 e0')
# --- P7 tessera gia' attiva su SCHUKO 1
put(0x331e,'rjmp CHKC','03 c1'); put(0x3336,'rjmp CHKC','f7 c0')
# --- P8 nuovo frame di stato
put(0x27b8,'jmp NEWSTAT','fe 01 75 96')
# --- P9 nuove uscite relè
put(0x36e0,'jmp OUTBLK','60 91 a1 02')
# --- P10 emergenza: arresto anche slot 2
put(0x253a,'call STOP12\nnop','81 e0 0e 94 cc 0d')
# --- P11 cmd 0x13 accetta indice 2
put(0x2c7e,'cpi r24,3','82 30')
# --- P12 LED: presa attiva
put(0x2aae,'call ANYRUN\nrjmp T2ACE','10 91 a1 02 11 23')
# --- P13 EEPROM slot 2
put(0x1b52,'call SAVEC','80 91 74 01'); put(0x1902,'jmp LOADC','44 23 09 f4')
# --- codice nuovo
code=open(P+'rf3.bin','rb').read()
assert len(d)<=0x3a00
d+=b'\xff'*(0x3a00-len(d))+code
assert len(d)<0x7000
open(P+'rfid3.bin','wb').write(d); print('ok',hex(len(d)),'reloc',n)
