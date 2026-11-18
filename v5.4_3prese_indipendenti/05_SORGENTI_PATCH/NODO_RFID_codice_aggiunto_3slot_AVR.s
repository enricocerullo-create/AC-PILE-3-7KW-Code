; ======================================================================
;  NODO RFID - versione 3 slot (AUTO + SCHUKO 1 + SCHUKO 2 indipendenti)
;  Codice aggiunto in flash libera a partire da 0x3A00
;  Slot 0 (0x02A1) = AUTO   (tessere tipo 1, kWh, contattore wallbox)
;  Slot 1 (0x02B9) = SCHUKO 2 (tessere tipo 3, minuti, uscita D5)
;  Slot 2 (0x02D1) = SCHUKO 1 (tessere tipo 3, minuti, uscita D2)  [NUOVO]
; ======================================================================
    .equ SA, 0x02A1
    .equ SB, 0x02B9
    .equ SC, 0x02D1
    .equ BUF, 0x03E8          ; buffer 22 byte (RAM azzerata all'avvio)
    .section .text
    .global _start
_start:

; ---- uscite relè (sostituisce il blocco a 0x36E0) ----------------------
OUTBLK:
    rcall RUNA                ; (non usato per relè, solo per coerenza)
    rcall RUNB
    mov  r16, r24             ; r16 = SCHUKO 2 attiva
    rcall RUNC
    mov  r22, r24             ; r22 = SCHUKO 1 attiva
    lds  r24, 0x0290          ; modalità non ancora ricevuta -> tutto OFF
    and  r24, r24
    brne 1f
    ldi  r22, 0
    ldi  r16, 0
1:  sts  0x028F, r1
    ldi  r24, 2
    call DW                   ; D2 = SCHUKO 1
    ldi  r22, 0
    ldi  r24, 4
    call DW                   ; D4 = OFF
    mov  r22, r16
    ldi  r24, 5
    call DW                   ; D5 = SCHUKO 2
    jmp  L2728

; ---- helper: slot attivo e non in pausa -> r24 = 0/1 (usa solo r24,r25)
RUNA: lds r24, SA
      and r24, r24
      breq 9f
      lds r25, SA+15
      rjmp 8f
RUNB: lds r24, SB
      and r24, r24
      breq 9f
      lds r25, SB+15
      rjmp 8f
RUNC: lds r24, SC
      and r24, r24
      breq 9f
      lds r25, SC+15
8:    ldi r24, 1
      and r25, r25
      breq 9f
      ldi r24, 0
9:    ret

; ---- S1: presa scelta occupata -> "Ricarica gia' in corso" + annulla ----
S1: ldi  r24, 2
    call SENDERR
    jmp  L2D78

; ---- NEWT3: tessera Schuko, rifiuta solo se SCHUKO 1 e 2 entrambe occupate
NEWT3:
    lds  r24, SB
    lds  r25, SC
    and  r24, r25
    breq 1f
    jmp  L35CC                ; entrambe occupate -> errore 2
1:  jmp  L37D4                ; prosegue (controllo credito, richiesta conferma)

; ---- CONF: scelta dello slot alla conferma (cmd 0x22) ------------------
;  tipo 1 -> slot 0 ; tipo 3: scelta 1 (SCHUKO 1) -> slot 2, scelta 2 -> slot 1
CONF:
    lds  r25, 0x026E
    ldi  r24, 0
    cpi  r25, 1
    breq 2f
    cpi  r25, 3
    brne 1f
    lds  r25, 0x01F6
    ldi  r24, 2
    cpi  r25, 1
    breq 2f
    ldi  r24, 1
    cpi  r25, 2
    breq 2f
1:  jmp  L2D78                ; scelta non valida -> annulla
2:  jmp  L2DA2

; ---- CHKC: tessera gia' attiva nello slot 2 (SCHUKO 1)? ----------------
CHKC:
    lds  r24, SC
    and  r24, r24
    breq 1f
    ldi  r20, 4
    ldi  r21, 0
    movw r22, r28
    subi r22, 0x95
    sbci r23, 0xFF
    ldi  r24, lo8(SC+1)
    ldi  r25, hi8(SC+1)
    call MEMCMP
    or   r24, r25
    brne 1f
    ldi  r24, 2
    jmp  L333A
1:  jmp  L3526

; ---- STOP12: arresto slot 1 e 2 (emergenza) -----------------------------
STOP12:
    ldi  r24, 1
    call STOPSLOT
    ldi  r24, 2
    jmp  STOPSLOT

; ---- ANYRUN: r17 = almeno una presa attiva (LED) ------------------------
ANYRUN:
    rcall RUNA
    mov  r17, r24
    rcall RUNB
    or   r17, r24
    rcall RUNC
    or   r17, r24
    ret

; ---- NEWSTAT: frame di stato tipo 1 (19 byte dati) ----------------------
;  AA 01 flags a(2) b(2) c(2) uidA(4) uidB(4) uidC(4) 55
;  flags: b0=AUTO b1=SCHUKO1 b2=SCHUKO2 b3=emergenza
NEWSTAT:
    push r16
    push r17
    ldi  r30, lo8(BUF)
    ldi  r31, hi8(BUF)
    ldi  r24, 22
1:  st   Z+, r1
    dec  r24
    brne 1b
    ldi  r30, lo8(BUF)
    ldi  r31, hi8(BUF)
    ldi  r24, 0xAA
    st   Z, r24
    ldi  r24, 0x01
    std  Z+1, r24
    ldi  r24, 0x55
    std  Z+21, r24
    ldi  r16, 0               ; flags
    ; --- AUTO (slot 0)
    rcall RUNA
    and  r24, r24
    breq 2f
    ori  r16, 0x01
    lds  r24, SA+6            ; a energia?
    and  r24, r24
    breq 2f
    lds  r22, SA+7
    lds  r23, SA+8
    lds  r24, SA+9
    lds  r25, SA+10
    ldi  r18, 10
    ldi  r19, 0
    ldi  r20, 0
    ldi  r21, 0
    call UDIV32               ; quoziente in r18..r21
    cp   r20, r1
    cpc  r21, r1
    breq 3f
    ldi  r18, 0xFF
    ldi  r19, 0xFF
3:  sts  BUF+3, r18
    sts  BUF+4, r19
2:  ; --- SCHUKO 1 (slot 2)
    rcall RUNC
    and  r24, r24
    breq 4f
    ori  r16, 0x02
    lds  r24, SC+7
    sts  BUF+5, r24
    lds  r24, SC+8
    sts  BUF+6, r24
4:  ; --- SCHUKO 2 (slot 1)
    rcall RUNB
    and  r24, r24
    breq 5f
    ori  r16, 0x04
    lds  r24, SB+7
    sts  BUF+7, r24
    lds  r24, SB+8
    sts  BUF+8, r24
5:  lds  r24, 0x0291          ; emergenza
    and  r24, r24
    breq 6f
    ori  r16, 0x08
6:  sts  BUF+2, r16
    ; --- UID (se slot attivo)
    ldi  r26, lo8(SA)
    ldi  r27, hi8(SA)
    ldi  r30, lo8(BUF+9)
    ldi  r31, hi8(BUF+9)
    rcall CPUID
    ldi  r26, lo8(SB)
    ldi  r27, hi8(SB)
    rcall CPUID
    ldi  r26, lo8(SC)
    ldi  r27, hi8(SC)
    rcall CPUID
    ; --- invio
    ldi  r20, 22
    ldi  r21, 0
    ldi  r22, lo8(BUF)
    ldi  r23, hi8(BUF)
    ldi  r24, 0x93
    ldi  r25, 0x03
    call SERWRITE
    pop  r17
    pop  r16
    jmp  L292C
; X = slot, Z = destinazione (avanza di 4)
CPUID:
    ld   r24, X+
    ldi  r25, 4
    and  r24, r24
    brne 2f
1:  st   Z+, r1
    dec  r25
    brne 1b
    ret
2:  ld   r24, X+
    st   Z+, r24
    dec  r25
    brne 2b
    ret

; ---- SAVEC: salva slot 2 in EEPROM (anello 4 record x 20 byte @0x260) ---
;  chiamato dentro la funzione di salvataggio originale (0x1B52),
;  0x0174 = indice record corrente, 0x0175 = numero sequenza
SAVEC:
    ldi  r30, lo8(BUF)
    ldi  r31, hi8(BUF)
    ldi  r24, 0x5C
    st   Z+, r24              ; [0] magic
    lds  r24, 0x0175
    st   Z+, r24              ; [1] seq
    ldi  r26, lo8(SC)
    ldi  r27, hi8(SC)
    ldi  r25, 15              ; [2..16] = slot+0..+14
1:  ld   r24, X+
    st   Z+, r24
    dec  r25
    brne 1b
    st   Z+, r1               ; [17]
    st   Z+, r1               ; [18]
    ldi  r30, lo8(BUF)
    ldi  r31, hi8(BUF)
    ldi  r24, 0
    ldi  r25, 19
2:  ld   r18, Z+
    eor  r24, r18
    dec  r25
    brne 2b
    st   Z, r24               ; [19] checksum xor
    ; indirizzo base = 0x260 + 20*idx
    lds  r24, 0x0174
    andi r24, 3
    ldi  r25, 20
    mul  r24, r25
    movw r14, r0
    clr  r1
    ldi  r24, 0x60
    add  r14, r24
    ldi  r24, 0x02
    adc  r15, r24
    ldi  r16, lo8(BUF)
    ldi  r17, hi8(BUF)
    ldi  r24, 20
    mov  r13, r24
3:  movw r24, r14
    call EERD
    movw r30, r16
    ld   r22, Z
    cp   r24, r22
    breq 4f
    movw r24, r14
    call EEWR
4:  ldi  r24, 1
    add  r14, r24
    adc  r15, r1
    subi r16, 0xFF
    sbci r17, 0xFF
    dec  r13
    brne 3b
    lds  r24, 0x0174          ; istruzione originale sostituita
    ret

; ---- LOADC: ripristino slot 2 da EEPROM (salto da 0x1902) ---------------
LOADC:
    mov  r2, r20              ; flag "slot ripristinato" originale
    lds  r24, 0x0174
    subi r24, 1
    andi r24, 3
    ldi  r25, 20
    mul  r24, r25
    movw r14, r0
    clr  r1
    ldi  r24, 0x60
    add  r14, r24
    ldi  r24, 0x02
    adc  r15, r24
    ldi  r16, lo8(BUF)
    ldi  r17, hi8(BUF)
    ldi  r24, 20
    mov  r13, r24
1:  movw r24, r14
    call EERD
    movw r30, r16
    st   Z, r24
    ldi  r24, 1
    add  r14, r24
    adc  r15, r1
    subi r16, 0xFF
    sbci r17, 0xFF
    dec  r13
    brne 1b
    ; verifica
    ldi  r30, lo8(BUF)
    ldi  r31, hi8(BUF)
    ld   r24, Z
    cpi  r24, 0x5C
    brne 9f
    ldd  r24, Z+1
    lds  r25, 0x0175
    cp   r24, r25
    brne 9f
    ldi  r24, 0
    ldi  r25, 19
2:  ld   r18, Z+
    eor  r24, r18
    dec  r25
    brne 2b
    ld   r18, Z
    cp   r24, r18
    brne 9f
    lds  r24, BUF+2           ; slot attivo?
    and  r24, r24
    breq 9f
    ; ripristina: +0..+14 dal record, +15 pausa=0, +16..+23 = millis
    ldi  r30, lo8(BUF+2)
    ldi  r31, hi8(BUF+2)
    ldi  r26, lo8(SC)
    ldi  r27, hi8(SC)
    ldi  r25, 15
3:  ld   r24, Z+
    st   X+, r24
    dec  r25
    brne 3b
    ldi  r24, 1
    sts  SC, r24
    lds  r24, SC+6            ; flag energia normalizzato 0/1
    and  r24, r24
    breq 5f
    ldi  r24, 1
5:  sts  SC+6, r24
    st   X+, r1               ; +15 pausa
    st   X+, r4
    st   X+, r5
    st   X+, r6
    st   X+, r7               ; +16..19
    st   X+, r4
    st   X+, r5
    st   X+, r6
    st   X+, r7               ; +20..23
    ldi  r24, 1
    mov  r2, r24
9:  mov  r20, r2
    and  r20, r20
    breq 8f
    jmp  L1908
8:  jmp  L1634
