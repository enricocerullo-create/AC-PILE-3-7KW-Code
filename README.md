# AC PILE 3-7 kW – conversione prese

Colonnina di ricarica con WALLBOX (STM32F103), nodo RFID (ATmega328P a 8 MHz, MFRC522)
e display Nextion 800x480.
Configurazione originale: **Tipo 2 + Scame + Schuko**.
Questo repository contiene i firmware originali e le versioni modificate: sostituzione della Scame con una seconda Schuko (5.4)
oppure solo ricarica Auto, con le Schuko disattivate (5.5).
I sorgenti originali non erano disponibili, quindi le modifiche sono **patch binarie** sui firmware compilati.

## Contenuto

| Cartella | Contenuto |
|---|---|
| `originali/` | Firmware e progetto display originali (WALLBOX v5.1, nodo RFID, programmatore card `rfid_bridge`, `nextion.HMI`) |
| `v5.4_3prese_indipendenti/` | **Versione consigliata.** Tipo 2 + SCHUKO 1 + SCHUKO 2 attive contemporaneamente se libere |
| `v5.5_solo_auto/` | **Solo ricarica AUTO (Tipo 2).** SCHUKO 1 e SCHUKO 2 disattivate, card Schuko rifiutate. Derivata dalla 5.4 |

Ogni versione contiene:
- `00_LEGGIMI_PRIMA.txt` – riepilogo rapido;
- `01_`/`02_` – i file .hex da caricare;
- `03_DISPLAY_NEXTION` – il nuovo sfondo della pagina "tempi" (immagine ID 3, da sostituire nel Nextion Editor);
- `04_DOCUMENTAZIONE` – istruzioni complete, cablaggio, elenco delle modifiche, collaudo;
- `05_SORGENTI_PATCH` – codice assembly aggiunto e script Python che applicano le patch;
- `06_TEST_SIMULATORE` – nella 5.4 e nella 5.5: banco di prova `simavr` del nodo RFID, con card MIFARE emulata, e relativi risultati.

## Riepilogo versione 5.4
- **SCHUKO 1** = uscita **D2** del nodo RFID (nuova presa al posto della Scame).
  **SCHUKO 2** = uscita **D5** (Schuko esistente).
  **D4** non è più usata.
- **Card:** Auto = tipo 1 (credito in kWh), Schuko = tipo 3 (credito in minuti).
  La presa Schuko si sceglie sul display (pagina "CONFERMA SELEZIONE").
  Le card Scame (tipo 2) vanno riprogrammate. Il programmatore `rfid_bridge` non va modificato.
- **Aggiornamento:** WALLBOX e nodo RFID vanno aggiornati **insieme**.
  Sulla WALLBOX non usare "Full chip erase": la pagina 0x0800FC00 (impostazioni) va preservata.
- **Potenza:** con le tre prese attive insieme verificare linea e protezioni (circa 32 A + 16 A + 16 A).
- **Display:** nel Nextion Editor sostituire l'immagine ID 3, ricompilare e caricare il .tft.
- **Versione mostrata** nella pagina statistiche: 5.4.

## Riepilogo versione 5.5 (solo AUTO)
- Resta attiva solo la **Tipo 2** con card tipo 1 (kWh).
- Uscite **D2, D4, D5** del nodo RFID sempre spente.
- Card Schuko (tipo 3) e Scame (tipo 2): **"Operazione non disponibile"**. La card non viene modificata.
- La pagina "CONFERMA SELEZIONE" non compare più. Nella pagina "tempi" i campi Schuko restano vuoti.
- **Display:** nel Nextion Editor sostituire l'immagine ID 3 con quella della 5.5 (solo riquadro AUTO).
- **Aggiornamento:** WALLBOX, nodo RFID e display insieme, senza sessioni aperte. Protocollo uguale alla 5.4.
- **Versione mostrata** nella pagina statistiche: 5.5.
- Lo script `v5.5_solo_auto/05_SORGENTI_PATCH/SOLO_AUTO_applica_patch_v5.5.py` genera la 5.5 dai .hex della 5.4.

## Nota sugli script di patch
Gli script in `05_SORGENTI_PATCH` documentano esattamente quali byte vengono modificati.
Sono stati eseguiti in una cartella di lavoro temporanea: i percorsi al loro interno vanno adattati
(firmware originale estratto in binario con `objcopy -O binary`).
Per rigenerare il codice servono `arm-none-eabi-binutils`, `binutils-avr` e Python 3.

## Stato
- **Nodo RFID:** provato in simulazione (`simavr`).
- **WALLBOX e display:** verificati solo su carta.
- Il collaudo sulla colonnina descritto in `04_DOCUMENTAZIONE` è **obbligatorio** prima dell'uso pubblico.
