; Blocco uscite relè riscritto (0x36E0 .. max 0x3797)
;  D2 = SCHUKO 1 (slot A, solo tessere tipo 3)
;  D4 = non usato (sempre OFF)  [era Scame]
;  D5 = SCHUKO 2 (slot B)
    .section .text
    .global outblk
outblk:
    lds  r22, 0x02A1
    and  r22, r22
    breq 1f
    lds  r22, 0x02B0
    ldi  r24, 1
    eor  r22, r24          ; r22 = slot A attivo e non in pausa
1:  lds  r16, 0x02B9
    and  r16, r16
    breq 2f
    lds  r16, 0x02C8
    ldi  r24, 1
    eor  r16, r24          ; r16 = slot B attivo e non in pausa
2:  lds  r24, 0x0290       ; modalita' non ancora ricevuta -> tutto OFF
    and  r24, r24
    brne 3f
    ldi  r22, 0
    ldi  r16, 0
3:  lds  r25, 0x02A6       ; tipo tessera slot A
    cpi  r25, 3
    breq 4f
    ldi  r22, 0            ; slot A = AUTO -> nessun relè (lo gestisce la wallbox)
4:  sts  0x028F, r1
    ldi  r24, 2
    call DW                ; digitalWrite(2, r22)
    ldi  r22, 0
    ldi  r24, 4
    call DW                ; digitalWrite(4, LOW)
    mov  r22, r16
    ldi  r24, 5
    call DW                ; digitalWrite(5, r16)
    jmp  L2728
; ---- S1: presa scelta occupata -> messaggio "Ricarica gia' in corso" + annulla
    .global S1
S1: ldi  r24, 2
    call SENDERR
    rjmp L2D78
; ---- S2: campo "b" del frame stato = minuti residui slot A (Schuko 1)
    .global S2
S2: lds  r24, 0x02A7       ; slot A a energia (AUTO)?
    and  r24, r24
    brne 5f
    lds  r18, 0x02A8
    lds  r19, 0x02A9
    std  Y+27, r18
    std  Y+28, r19
5:  rjmp L282A
