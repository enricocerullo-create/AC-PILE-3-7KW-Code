import sys; sys.path.insert(0,'.')
from hexio import *
S='/tmp/claude-0/-home-user/a37cbca1-efba-546d-9cb2-ab23f277aad5/scratchpad/'
# WALLBOX
mem,start=read_hex(S+'work/wallbox.hex'); B=0x08000000
new=open(S+'patch/wallbox_new.bin','rb').read()
lo=min(mem); hi=max(mem); print('wb orig range',hex(lo),hex(hi),len(mem), 'contig',hi-lo+1==len(mem))
ch=0
for a in list(mem):
    if mem[a]!=new[a-B]: mem[a]=new[a-B]; ch+=1
for a in range(B+0xA000,B+len(new)): mem[a]=new[a-B]; ch+=1
print('wb changed bytes',ch)
write_hex(S+'out/WALLBOX_20261117_2SCHUKO.hex',mem,start)
# RFID
mem,start=read_hex(S+'work/rfid.hex')
new=open(S+'patch/rfid_new.bin','rb').read()
print('rf range',hex(min(mem)),hex(max(mem)),len(mem))
ch=0
for a in list(mem):
    if mem[a]!=new[a]: mem[a]=new[a]; ch+=1
print('rf changed bytes',ch)
write_hex(S+'out/rfid_node_20261117_2SCHUKO.hex',mem,start,ext=False)
