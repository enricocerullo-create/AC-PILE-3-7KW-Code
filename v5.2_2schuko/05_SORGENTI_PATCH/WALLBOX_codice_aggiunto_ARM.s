    .syntax unified
    .cpu cortex-m3
    .thumb
    .text
    .global _start
_start:
@ ---------------------------------------------------------------
@ TR1  @0x0800A000 : tempi.eneScame (ora SCHUKO 1) = minuti (u16 in r0)
@      chiamato con r0 = minuti (campo "b" del frame stato RFID)
TR1:
    mov   r1, r0
    ldr   r0, =0x080091C0          @ "tempi.eneScame.txt=\""
    b.w   f_send_u16               @ 0x08002444 (prefisso, valore)
    .align 2
    .ltorg
@ ---------------------------------------------------------------
@ TR2 : frame tipo 2 (tessera in attesa di conferma)
@   in: r0=1 r1=0 r2 = 4 se tessera Schuko (tipo 3), 1 altrimenti
@   tipo 1 (auto): comportamento originale -> conferma automatica
@   tipo 3 (schuko): mostra "page conferma" e attende SCHUKO 1/2
    .align 2
TR2:
    cmp   r2, #4
    bne.w f_confirm                @ 0x08002ea8 (tail call, LR originale)
    push  {r4, lr}
    ldr   r3, =0x20001E40          @ conferma 0x22 gia' in invio? ignora
    ldrb  r3, [r3]
    cbnz  r3, 1f
    ldr   r4, =0x20001E30          @ flag "attesa scelta presa"
    ldrb  r3, [r4]
    cbnz  r3, 1f                   @ pagina gia' mostrata
    movs  r3, #1
    strb  r3, [r4]
    bl    f_millis                 @ 0x08005204
    ldr   r3, =0x20001E3C          @ timer timeout conferma (30 s, gestito dal fw originale)
    str   r0, [r3]
    ldr   r0, =0x08009148          @ "page conferma"
    bl    f_page                   @ 0x08002394
1:  pop   {r4, pc}
    .align 2
    .ltorg
@ ---------------------------------------------------------------
@ TR6 : abilitazione EVSE (0x20000b2d) solo per sessione AUTO
@   [r7+0x4d] = bit0 (slot A attivo) AND NOT bit1 (slot A = Schuko 1)
    .align 2
TR6:
    ldrb  r3, [r7, #0x0c]
    ldrb  r2, [r7, #0x0d]
    bics  r3, r2
    strb.w r3, [r7, #0x4d]
    bx    lr
