    .syntax unified
    .cpu cortex-m3
    .thumb
    .text
    .global _start
    .equ UIDC, 0x20004000      @ UID sessione SCHUKO 1 (slot 2 del nodo RFID)
_start:
@ TR1 : tempi.eneScame (= SCHUKO 1) mostra i minuti (r0)
TR1:
    mov   r1, r0
    ldr   r0, =0x080091C0          @ "tempi.eneScame.txt=\""
    b.w   f_send_u16
    .ltorg
@ TR2 : tessera Schuko (tipo 3) -> pagina "conferma", altrimenti conferma automatica
    .align 2
TR2:
    cmp   r2, #4
    bne.w f_confirm
    push  {r4, lr}
    ldr   r3, =0x20001E40
    ldrb  r3, [r3]
    cbnz  r3, 1f
    ldr   r4, =0x20001E30
    ldrb  r3, [r4]
    cbnz  r3, 1f
    movs  r3, #1
    strb  r3, [r4]
    bl    f_millis
    ldr   r3, =0x20001E3C          @ timeout 30 s del firmware originale
    str   r0, [r3]
    ldr   r0, =0x08009148          @ "page conferma"
    bl    f_page
1:  pop   {r4, pc}
    .ltorg
@ TRP : parser frame di stato (19 byte) + copia UID slot 2 (byte 15..18)
    .align 2
TRP:
    push  {r4, lr}
    mov   r4, r0
    bl    f_parse
    cbz   r0, 1f
    ldr   r3, =UIDC
    ldrb  r2, [r4, #15]
    strb  r2, [r3, #0]
    ldrb  r2, [r4, #16]
    strb  r2, [r3, #1]
    ldrb  r2, [r4, #17]
    strb  r2, [r3, #2]
    ldrb  r2, [r4, #18]
    strb  r2, [r3, #3]
1:  pop   {r4, pc}
    .ltorg
@ TRU : tempi.idRfidScame (= SCHUKO 1) -> r3 = puntatore UID se attiva, altrimenti 0
    .align 2
TRU:
    ldrb  r3, [r7, #0x0d]
    cbz   r3, 1f
    ldr   r3, =UIDC
    ldr   r2, [r3]
    cbnz  r2, 1f
    movs  r3, #0
1:  bx    lr
    .ltorg
@ TRE : ripresa dopo blackout, UID da mostrare: AUTO se attiva, altrimenti SCHUKO 1
    .align 2
TRE:
    ldrb  r3, [r7, #0x0c]
    cbz   r3, 1f
    ldr.w r3, [r7, #0x16]
    bx    lr
1:  ldr   r3, =UIDC
    ldr   r3, [r3]
    bx    lr
    .ltorg
@ TRN : "NO" alla ripresa -> arresta anche lo slot 2 (r0 = 1 all'ingresso)
    .align 2
TRN:
    push  {r4, lr}
    bl    f_stop13                 @ 0x13(1)
    movs  r0, #0x32
    bl    f_delay
    movs  r0, #2
    bl    f_stop13                 @ 0x13(2)
    pop   {r4, pc}
@ TRV : popup "minuti esauriti" SCHUKO 1 -> r3 = puntatore UID SCHUKO 1 se presente, altrimenti 0
@       (il firmware originale usava l'UID della sessione 0, che ora e' l'AUTO)
    .align 2
TRV:
    ldr   r3, =UIDC
    ldr   r2, [r3]
    cbnz  r2, 1f
    movs  r3, #0
1:  bx    lr
    .ltorg
