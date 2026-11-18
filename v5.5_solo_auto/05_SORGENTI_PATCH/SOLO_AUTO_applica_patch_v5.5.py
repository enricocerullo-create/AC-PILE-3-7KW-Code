# Versione 5.5 "SOLO AUTO": parte dai firmware v5.4 (3 prese) e disabilita SCHUKO 1 e SCHUKO 2.
# Uso: python3 SOLO_AUTO_applica_patch_v5.5.py WALLBOX_v5.4.hex NODO_RFID_v5.4.hex WALLBOX_out.hex NODO_out.hex
import sys, struct, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hexio import read_hex, write_hex

def put(mem, addr, new, expect, nota):
    old = bytes(mem[addr + i] for i in range(len(expect)))
    assert old == expect, (nota, hex(addr), old.hex(), expect.hex())
    assert len(new) == len(expect), nota
    for i, b in enumerate(new): mem[addr + i] = b
    print(f'  {addr:#010x}  {old.hex():<10} -> {new.hex():<10} {nota}')

H = lambda *w: struct.pack('<' + 'H' * len(w), *w)
def bl(src, dst):                      # Thumb-2 BL
    off = dst - (src + 4); s = 1 if off < 0 else 0; off &= (1 << 25) - 1
    i1 = (off >> 23) & 1; i2 = (off >> 22) & 1
    j1 = (1 - i1) ^ s; j2 = (1 - i2) ^ s
    return H(0xF000 | (s << 10) | ((off >> 12) & 0x3ff), 0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7ff))
def rjmp(src, dst):                    # AVR RJMP
    k = (dst - (src + 2)) // 2; assert -2048 <= k < 2048; return H(0xC000 | (k & 0xfff))

wb_in, rf_in, wb_out, rf_out = sys.argv[1:5]

# ------------------------------------------------------------ WALLBOX (STM32)
wb, wb_start = read_hex(wb_in)
print('WALLBOX')
# W1 tessera in attesa: niente piu' pagina "conferma" (SCHUKO 1/2), torna la conferma automatica originale
put(wb, 0x08003e32, bl(0x08003e32, 0x08002ea8), bl(0x08003e32, 0x0800a00c), 'W1 pagina conferma disattivata')
# W2..W5 pagina tempi: SCHUKO 1 e SCHUKO 2 sempre considerate non attive
put(wb, 0x08003be4, H(0x2300), H(0x7b7b), 'W2 eneScame  (SCHUKO 1) sempre vuoto')
put(wb, 0x08003c2e, H(0x2300), H(0x7bbb), 'W3 eneSchuko (SCHUKO 2) sempre vuoto')
put(wb, 0x08003ca6, H(0x2300, 0xbf00), bl(0x08003ca6, 0x0800a068), 'W4 idRfidScame  sempre vuoto')
put(wb, 0x08003cca, H(0x2300), H(0x7bbb), 'W5 idRfidSchuko sempre vuoto')
# W6 campi energia SCHUKO vuoti: stringa "" invece di "----" (la Tipo 2 resta "----")
put(wb, 0x080025e4, struct.pack('<I', 0x080091f8), struct.pack('<I', 0x080091f0), 'W6a eneScame  -> ""')
put(wb, 0x08002600, struct.pack('<I', 0x080091f8), struct.pack('<I', 0x080091f0), 'W6b eneSchuko -> ""')
assert bytes(wb[0x080091f8 + i] for i in range(1)) == b'\0'
# W7 versione
s = bytes(wb[0x08009130 + i] for i in range(22))
assert s == b'stat.version.txt="5.4"', s
put(wb, 0x08009130 + 18, b'5.5', b'5.4', 'W7 versione 5.5')
write_hex(wb_out, wb, wb_start, True)

# ------------------------------------------------------------ NODO RFID (ATmega328P)
rf, rf_start = read_hex(rf_in)
print('NODO RFID')
# R1 tessera Schuko (tipo 3), in ogni modalita': errore 8 "Operazione non disponibile"
put(rf, 0x35c0, bytes.fromhex('88e0') + rjmp(0x35c2, 0x3574), bytes.fromhex('0c94381d'), 'R1 tessera tipo 3 -> errore 8')
# R2 conferma: tipo 3 annullata sempre (nessuno slot Schuko puo' partire)
put(rf, 0x3a8e, rjmp(0x3a8e, 0x3aa2), bytes.fromhex('9330'), 'R2 conferma tipo 3 -> annulla')
# R3/R4 uscite: D2 (SCHUKO 1) e D5 (SCHUKO 2) sempre spente (D4 era gia' spenta)
put(rf, 0x3a04, bytes.fromhex('00e0'), bytes.fromhex('082f'), 'R3 D5 sempre OFF')
put(rf, 0x3a08, bytes.fromhex('60e0'), bytes.fromhex('682f'), 'R4 D2 sempre OFF')
write_hex(rf_out, rf, rf_start, False)
print('ok')
