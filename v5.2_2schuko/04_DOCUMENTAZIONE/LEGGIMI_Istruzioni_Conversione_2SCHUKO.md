# Conversione: Tipo 2 + Scame + Schuko → Tipo 2 + SCHUKO 1 + SCHUKO 2

Non avevo i sorgenti, quindi le modifiche sono **patch binarie** applicate ai firmware originali
(`WALLBOX_20260918.hex` v5.1 e `rfid_node_20260923.hex`). Ogni byte modificato è elencato qui sotto
e gli script in `sorgenti_patch/` rigenerano gli stessi .hex partendo dagli originali.

## File

| File | Cosa contiene |
|---|---|
| `firmware/WALLBOX_20261117_2SCHUKO.hex` | Firmware della scheda STM32 (wallbox/display), versione mostrata: **5.2** |
| `firmware/rfid_node_20261117_2SCHUKO.hex` | Firmware del nodo RFID (ATmega328P) |
| `display/pic3_tempi_SCHUKO1_SCHUKO2_800x480.png` | Nuovo sfondo della pagina "tempi" (immagine ID 3) |
| `display/confronto_prima_dopo.png` | Confronto tra la pagina prima e dopo |
| `sorgenti_patch/` | Sorgenti assembly delle patch e script Python che generano gli .hex |

## Come funziona il nuovo sistema

Il nodo RFID ha **due slot** di ricarica e la vecchia logica li usava così:
slot A = Auto **oppure** Scame, slot B = Schuko.
Il firmware conteneva già quasi tutto il necessario per due prese Schuko (la "config2"):
la modalità 2 nel nodo RFID, i tasti SCHUKO 1/SCHUKO 2 nella pagina `conferma` e la loro gestione nella wallbox.
Questa logica però era disattivata. Le patch la attivano e la uniscono alla presa Tipo 2:

| Presa | Slot RFID | Uscita nodo RFID | Tessere accettate | Credito |
|---|---|---|---|---|
| Tipo 2 (AUTO) | A | nessuna (contattore wallbox PC3, come prima) | tipo 1 | kWh |
| **SCHUKO 1** (sostituisce la Scame) | A | **D2** | tipo 3 (Schuko) | minuti |
| **SCHUKO 2** (la Schuko esistente) | B | **D5** (invariata) | tipo 3 (Schuko) | minuti |
| (D4, ex Scame) | – | sempre spenta | – | – |

**Uso:** quando si avvicina una tessera Schuko, il display mostra la pagina **CONFERMA SELEZIONE**.
L'utente tocca SCHUKO 1 o SCHUKO 2 e si attiva solo la presa scelta.
Se l'utente non sceglie entro 30 s, la richiesta viene annullata e il display torna alla pagina precedente.
Le tessere Auto funzionano come prima, senza conferma.

### ⚠️ Limitazione (è la stessa della vecchia Scame)
SCHUKO 1 e AUTO usano **lo stesso slot**, quindi **non possono essere attive contemporaneamente**.
Se si tenta, il display mostra "Ricarica / gia' in corso". SCHUKO 2 resta sempre indipendente.
Per avere tre prese tutte indipendenti servirebbe un terzo slot nel nodo RFID: è una modifica molto più
estesa (memoria, EEPROM, protocollo) e non l'ho fatta.

## Modifiche hardware / cablaggio
1. Togliere la presa Scame e installare la nuova Schuko (**SCHUKO 1**), alimentata tramite il **relè comandato da D2**
   del nodo RFID, con una linea propria e protetta (magnetotermico + differenziale adeguati).
   È lo stesso schema che il firmware originale usava nella "config2" (due Schuko su D2 e D5).
2. Il relè/circuito su **D4** non viene più comandato. Se scollegava o commutava il pilota CP della Tipo 2
   verso la Scame, ripristinare il collegamento diretto della Tipo 2.
3. La Schuko esistente (D5) non cambia e diventa **SCHUKO 2**: aggiornare le etichette sulla colonnina.

> Non conosco lo schema elettrico reale. Fate verificare i punti 1 e 2 a un elettricista prima di mettere in servizio.

## Programmazione

**WALLBOX (STM32F103, base 0x08000000)**
Con ST-Link / STM32CubeProgrammer, caricare l'hex **senza "Full chip erase"**: va bene la cancellazione solo
delle pagine scritte.
La pagina di configurazione a **0x0800FC00** (impostazioni, statistiche, seriale) non viene toccata
e deve essere preservata.
Il codice nuovo occupa 0x0800A000–0x0800A04F.

**Nodo RFID (ATmega328P)**
Caricare con lo stesso metodo usato finora (bootloader Arduino o ISP).
Con ISP, la cancellazione del chip azzera anche la EEPROM, a meno che il fuse EESAVE sia attivo.
Eseguire l'aggiornamento **senza sessioni attive o sospese**: una vecchia sessione Scame salvata
verrebbe ripresa come se fosse una sessione Auto.

**Aggiornare entrambi i firmware insieme:** il nuovo nodo RFID con la vecchia wallbox (o viceversa)
non funziona correttamente.

## Display Nextion
Non ho modificato il file .HMI. Le pagine interne hanno un checksum non standard e le immagini compilate
usano un formato proprietario: un file modificato a mano rischierebbe di non aprirsi più nel Nextion Editor.
Le modifiche richiedono pochi minuti nel Nextion Editor:

1. Aprire `nextion.HMI`.
2. Nel pannello **Picture**, selezionare l'immagine **ID 3** (sfondo della pagina `tempi`, "CREDITO CARTA RFID"),
   scegliere **Replace** e caricare `display/pic3_tempi_SCHUKO1_SCHUKO2_800x480.png`.
   L'ID resta 3, quindi la pagina non va toccata.
   I campi `eneScame`/`idRfidScame` adesso mostrano la SCHUKO 1 (in minuti) e `eneSchuko`/`idRfidSchuko` la SCHUKO 2.
3. *(Consigliato)* Nella pagina **`conferma`**, aggiungere un **Hotspot** sopra il pulsante disegnato "INDIETRO"
   (x=14, y=393, w=167, h=70).
   Nel *Touch Press Event* inserire: `printh 71 51 00 00 00 FF FF FF`
   In questo modo l'utente può annullare subito, senza aspettare i 30 s.
4. **Compile**, poi caricare il `.tft` sul display (scheda SD o upload seriale).

La pagina `conferma` (SCHUKO 1 / SCHUKO 2) esiste già nell'HMI e invia i codici giusti (`71 20` / `71 21`).

## Elenco patch

### WALLBOX (STM32)
| Indirizzo | Originale | Nuovo | Scopo |
|---|---|---|---|
| 0x08003A18 | `ldrb r3,[r7,#12]; strb.w r3,[r7,#0x4D]` | `bl 0x0800A044; nop` | Il Tipo 2 (EVSE) viene abilitato solo per sessioni AUTO, non per SCHUKO 1 |
| 0x08003C22 | `ldr r0,[r7,#0x40]; bl 0x08002560` | `ldrh r0,[r7,#0x12]; bl 0x0800A000` | Pagina tempi: SCHUKO 1 mostra i minuti residui al posto dei kWh |
| 0x08003E32 | `bl 0x08002EA8` | `bl 0x0800A00C` | Tessera Schuko: mostra `page conferma` e avvia il timeout di 30 s |
| 0x080092DC | "Ricarica vettura" | "Ricarica" | Messaggio "Ricarica gia' in corso" valido per tutte le prese |
| 0x08009144 | "5.1" | "5.2" | Versione mostrata nella pagina stat |
| 0x0800A000.. | – | codice nuovo (80 byte) | Funzioni TR1, TR2, TR6 (vedi `wb_tramp.s`) |

Il comando di modalità inviato al nodo RFID resta 1.

### Nodo RFID (AVR)
| Indirizzo (byte) | Scopo |
|---|---|
| 0x2804 | Bit1 del frame di stato = "slot A è SCHUKO 1" (tipo 3); prima era "Scame" (tipo 2) |
| 0x2826 → 0x373C | Campo "b" del frame di stato = minuti residui di SCHUKO 1 |
| 0x2D88 | Lo slot viene scelto in base a SCHUKO 1/2 premuto sul display (come in config2) |
| 0x2DB4 → 0x3734 | Se la presa scelta è occupata: messaggio "gia' in corso" e annullamento |
| 0x3596–0x35A3 | Accettazione tessere: tipo 1 → Auto; tipo 3 → richiesta di scelta presa; tipo 2 (Scame) → "Operazione non disponibile" |
| 0x36E0–0x3797 | Uscite riscritte: D2 = SCHUKO 1, D4 = sempre OFF, D5 = SCHUKO 2; Auto non usa relè del nodo |

## Prove da fare prima della messa in servizio
- [ ] Tessera Schuko → compare la pagina CONFERMA → SCHUKO 1 → si chiude solo il relè D2; la pagina "tempi" mostra i minuti sotto SCHUKO 1.
- [ ] Stessa prova con SCHUKO 2 → solo D5.
- [ ] Nessuna scelta per 30 s → torna indietro e non si attiva nulla.
- [ ] Ripassando la tessera su una presa attiva → la sessione si chiude e il credito residuo viene riscritto sulla tessera.
- [ ] Tessera Auto → ricarica Tipo 2 come prima (kWh), D2 e D4 restano spenti.
- [ ] Auto attiva + tessera Schuko → SCHUKO 1 dà "gia' in corso", SCHUKO 2 funziona.
- [ ] SCHUKO 1 attiva + veicolo collegato alla Tipo 2 → il contattore della Tipo 2 **non** si chiude.
- [ ] Vecchia tessera Scame → "Operazione non disponibile".
- [ ] Emergenza e perdita di comunicazione → tutte le prese si spengono.
