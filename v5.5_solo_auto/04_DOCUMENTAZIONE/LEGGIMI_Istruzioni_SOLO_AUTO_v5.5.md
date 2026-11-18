# Versione 5.5: solo ricarica AUTO (Tipo 2), Schuko eliminate

La 5.5 parte dalla 5.4 (Tipo 2 + SCHUKO 1 + SCHUKO 2) e **disabilita le due prese Schuko**.
Resta attiva solo la ricarica AUTO sulla presa Tipo 2, con le tessere tipo 1 (credito in kWh).
Come nelle versioni precedenti, le modifiche sono **patch binarie**: i sorgenti originali non sono disponibili.
Sono pochissimi byte (16 nella WALLBOX, 10 nel nodo RFID) e il protocollo tra le due schede è quello della 5.4.

## Funzionamento

| Presa | Stato | Uscita | Tessere |
|---|---|---|---|
| Tipo 2 (AUTO) | **attiva**, come prima | contattore della wallbox (PC3) | tipo 1, kWh |
| SCHUKO 1 | **disabilitata** | D2 sempre spenta | – |
| SCHUKO 2 | **disabilitata** | D5 sempre spenta | – |
| (D4) | non usata | sempre spenta | – |

- **Tessera Auto:** parte senza conferma, come prima. Ripassarla chiude la sessione e riscrive il credito residuo sulla tessera.
  Se l'AUTO è già occupata compare "Ricarica gia' in corso".
- **Tessera Schuko (tipo 3)** e **vecchia tessera Scame (tipo 2):** compare **"Operazione non disponibile"**.
  La tessera non viene modificata: il credito resta com'è.
- La pagina **CONFERMA SELEZIONE** (scelta SCHUKO 1 / SCHUKO 2) non compare più.
- **Blackout:** la pagina "riprendi" funziona come prima per l'AUTO.
- **Emergenza (D6) o allarme della wallbox:** la ricarica si ferma, come prima.
- Il programmatore RFID (rfid_bridge) **non va modificato**.

## Cablaggio
- La **Tipo 2** resta collegata direttamente alla wallbox: nessuna modifica.
- I relè su **D2** e **D5** non vengono più comandati (sempre aperti), quindi le prese Schuko restano senza tensione.
  Consigliato comunque: far **scollegare le prese Schuko**, oppure toglierle o etichettarle "fuori servizio".
  Le loro protezioni si possono lasciare aperte.
- La verifica della potenza per 3 prese insieme (circa 64 A) non serve più: torna valido il dimensionamento della sola Tipo 2.

Fate verificare il cablaggio da un elettricista.

## Programmazione
**Prima di aggiornare:** chiudere tutte le ricariche in corso, comprese eventuali sessioni Schuko,
ripassando le tessere. Dopo l'aggiornamento una tessera Schuko non viene più accettata.

1. **WALLBOX (STM32):**
   - caricare `WALLBOX_STM32_v5.5_SOLO_AUTO_20261118.hex` con ST-Link, **senza "Full chip erase"**:
     la pagina 0x0800FC00 con impostazioni, statistiche e seriale va preservata;
   - nella pagina statistiche la versione diventa **5.5**.
2. **NODO RFID (ATmega328P a 8 MHz):**
   - caricare `NODO_RFID_ATmega328P_v5.5_SOLO_AUTO_20261118.hex` con il metodo usato finora;
   - la sessione AUTO salvata in EEPROM resta valida.
3. **Aggiornare WALLBOX e NODO RFID insieme.** Il messaggio di stato è quello della 5.4 (19 byte).
   - Se la colonnina ha già la 5.3 o la 5.4, una scheda aggiornata comunica anche con l'altra non aggiornata.
     Le Schuko però risultano disabilitate solo quando sono aggiornate tutte e due.
   - Se la colonnina ha la 5.1 o la 5.2, aggiornarle **obbligatoriamente insieme**: il protocollo è cambiato dalla 5.3.
4. **Display Nextion:**
   - nel Nextion Editor, pagina `tempi`, sostituire l'immagine **ID 3** con
     `03_DISPLAY_NEXTION/NEXTION_Immagine_ID3_pagina_tempi_SOLO_AUTO_800x480.png`;
   - ricompilare e caricare il .tft.
   - Il riquadro AUTO resta **nella stessa posizione**, perché i campi di testo `eneAuto` e `idRfidAuto` sono posizionati sopra di esso.
     I riquadri SCHUKO 1 e SCHUKO 2 sono stati tolti.
   - La WALLBOX 5.5 invia testo vuoto ai campi `eneScame`, `idRfidScame`, `eneSchuko`, `idRfidSchuko`:
     se questi campi hanno sfondo "crop image" o colore uguale allo sfondo, non si vedono.
   - **Consigliato:** per nasconderli in ogni caso, aggiungere nel *Preinitialize Event* della pagina `tempi`:
     ```
     vis eneScame,0
     vis idRfidScame,0
     vis eneSchuko,0
     vis idRfidSchuko,0
     ```
     I nomi possono essere diversi nel vostro .HMI: controllateli nella pagina `tempi`.
   - La pagina `conferma` non viene più usata. Può restare nel progetto: l'Hotspot su INDIETRO consigliato nella 5.4 non serve più.

## Modifiche in dettaglio

### Nodo RFID (10 byte)
| Indirizzo | Prima (5.4) | Dopo (5.5) | Effetto |
|---|---|---|---|
| 0x35C0 | `jmp` gestione Schuko | `ldi r24,8` + `rjmp` invio errore | tessera tipo 3, in tutte le modalità → errore 8 "Operazione non disponibile" |
| 0x3A8E | `cpi r25,3` | `rjmp` annulla | alla conferma una tessera tipo 3 viene sempre annullata (doppia sicurezza) |
| 0x3A04 | `mov r16,r24` | `ldi r16,0` | uscita D5 (SCHUKO 2) sempre spenta |
| 0x3A08 | `mov r22,r24` | `ldi r22,0` | uscita D2 (SCHUKO 1) sempre spenta (D4 lo era già) |

### WALLBOX (16 byte)
| Indirizzo | Effetto |
|---|---|
| 0x08003E32 | tessera letta: ripristinata la conferma automatica originale, la pagina `conferma` non viene più aperta |
| 0x08003BE4, 0x08003C2E | pagina tempi: SCHUKO 1 e SCHUKO 2 sempre considerate non attive |
| 0x08003CA6, 0x08003CCA | UID SCHUKO 1 e SCHUKO 2 sempre vuoti |
| 0x080025E4, 0x08002600 | campi energia SCHUKO vuoti `""` invece di `----` (AUTO invariata) |
| 0x08009142 | versione 5.5 |

Il contattore della Tipo 2 non viene mai bloccato dalle Schuko (flag "Scame" forzato a 0 già dalla 5.3).

### Rigenerare i file
`05_SORGENTI_PATCH/SOLO_AUTO_applica_patch_v5.5.py` applica le modifiche ai .hex della 5.4 e,
prima di scrivere, controlla che ogni byte originale sia quello atteso:
```
python3 SOLO_AUTO_applica_patch_v5.5.py WALLBOX_5.4.hex NODO_RFID_5.4.hex WALLBOX_5.5.hex NODO_RFID_5.5.hex
```
I sorgenti della 5.4 (3 prese), su cui si basa la 5.5, sono in `v5.4_3prese_indipendenti/05_SORGENTI_PATCH/`.
I .hex di partenza sono quelli di `v5.4_3prese_indipendenti/01_...` e `02_...`.

## Prove in simulazione (nodo RFID)
Il firmware del nodo è stato eseguito nel simulatore AVR `simavr` con una tessera MIFARE emulata
(lettura e scrittura reali dei blocchi cifrati), la seriale verso la wallbox, l'EEPROM e i pin.
Risultati in `06_TEST_SIMULATORE/TEST_risultati_SOLO_AUTO.txt`: **19 verifiche su 19 superate**.

| Prova | Esito |
|---|---|
| Tessera AUTO: sessione avviata, flag e UID corretti, D2/D4/D5 spente | ✅ |
| Tessera Schuko + scelta SCHUKO 1: errore 8, tessera non scritta, nessuna presa attivata, AUTO ancora attiva | ✅ |
| Tessera Schuko + scelta SCHUKO 2: errore 8, tessera non scritta, uscite spente | ✅ |
| Vecchia tessera Scame (tipo 2): errore 8 | ✅ |
| Conferma forzata di una tessera tipo 3 con scelta 1 e 2: nessuna Schuko avviata | ✅ |
| Sessioni Schuko residue forzate in RAM: D2 e D5 restano spente, chiudibili con lo stop 0x13 | ✅ |
| Ripasso tessera AUTO: sessione chiusa e credito riscritto sulla tessera | ✅ |
| Nuova sessione AUTO dopo la chiusura | ✅ |
| Emergenza: tutto fermo | ✅ |

Controllo del banco di prova: lo stesso test eseguito sul nodo **5.4** fallisce 9 verifiche
(le Schuko partono), quindi il test distingue davvero le due versioni.

**Non** è stato possibile simulare la WALLBOX (STM32 + display): le sue modifiche sono state verificate solo
al disassemblatore. Il collaudo sulla colonnina è quindi indispensabile.

## Collaudo sulla colonnina (obbligatorio)
- [ ] Pagina statistiche: versione **5.5**.
- [ ] Tessera Auto: la ricarica parte, il veicolo carica, il display è corretto.
- [ ] Pagina "CREDITO CARTA RFID": solo il riquadro AUTO, con UID e kWh corretti; nessun testo dove c'erano le Schuko.
- [ ] Ripassare la tessera Auto: la ricarica si chiude e il credito residuo torna sulla tessera.
- [ ] Credito Auto esaurito: la ricarica si ferma.
- [ ] Tessera Schuko: "Operazione non disponibile", nessuna pagina di scelta, nessun relè che scatta, credito della tessera invariato.
- [ ] Vecchia tessera Scame: "Operazione non disponibile".
- [ ] Prese Schuko senza tensione in ogni situazione (misurare con il tester).
- [ ] Togliere corrente durante una ricarica Auto: al ritorno compare "riprendi"; provare sia SÌ sia NO.
- [ ] Emergenza: tutto spento.
