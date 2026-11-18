import sys; sys.path.insert(0,'.')
from hexio import *
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
def build(orig,newbin,out,base,newstart,ext):
    mem,start=read_hex(S+'work/'+orig)
    new=open(newbin,'rb').read(); ch=0
    for a in list(mem):
        if mem[a]!=new[a-base]: mem[a]=new[a-base]; ch+=1
    for a in range(base+newstart,base+len(new)): mem[a]=new[a-base]; ch+=1
    write_hex(out,mem,start,ext); print(out.split('/')[-1],'byte cambiati',ch)
build('wallbox.hex',S+'p3/wallbox3.bin',S+'p3/WALLBOX3.hex',0x08000000,0xA000,True)
build('rfid.hex',S+'p3/rfid3.bin',S+'p3/RFID3.hex',0,0x3A00,False)
