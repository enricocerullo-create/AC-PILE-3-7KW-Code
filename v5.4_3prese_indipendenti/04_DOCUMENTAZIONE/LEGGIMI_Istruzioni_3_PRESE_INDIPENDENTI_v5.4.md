# Versione 5.4: Tipo 2 + SCHUKO 1 + SCHUKO 2 tutte indipendenti

## Novità della 5.4 (cambia solo il firmware WALLBOX)
Corretto il popup **"RICARICA TERMINATA PER ID NR."** che, chiudendo la SCHUKO 1 con la sua card,
compariva con l'UID della card **AUTO**.
**Causa:** nel firmware originale era rimasto un controllo della vecchia configurazione a 2 Schuko.
Quando i minuti della prima presa scendevano a 0, mostrava il popup con l'UID della prima sessione,
che nella versione a 3 prese è l'AUTO. La ricarica AUTO non veniva interrotta: era solo il messaggio a essere sbagliato.
**Correzione:** il controllo ora usa l'UID della SCHUKO 1.
Il nodo RFID è lo stesso della 5.3: se lo avete già aggiornato, ricaricate **solo la WALLBOX**.
Nella pagina statistiche la versione mostrata diventa **5.4**.

**Simulazione:** il nodo RFID è stato provato anche con una card MIFARE emulata (lettura e scrittura reali dei blocchi cifrati).
SCHUKO 1 è stata chiusa più volte, con card tenuta 150 ms, 600 ms e 2,5 s, mentre AUTO e SCHUKO 2 erano attive:
l'AUTO è rimasta sempre attiva e il credito è stato riscritto correttamente sulla card
(vedi `06_TEST_SIMULATORE/TEST_risultati_chiusura_SCHUKO1_con_card.txt`).


Questa versione sostituisce la 5.2 e la 5.3. Le tre prese funzionano **contemporaneamente**, purché ciascuna sia libera.
Senza i sorgenti originali, le modifiche sono **patch binarie** ai firmware originali
(`WALLBOX_20260918.hex` v5.1 e `rfid_node_20260923.hex`).

## ⚠️ Prima di tutto: potenza disponibile
Prima le prese Auto e Scame si escludevano a vicenda. Ora le tre prese possono assorbire insieme, per esempio:
Tipo 2 a 7 kW (circa 32 A) + 2 Schuko a 3,6 kW (circa 16 A ciascuna) = **circa 64 A**.
Verificate contatore, linea di alimentazione, sezione dei cavi e protezioni.
Se la potenza non è sufficiente, **non installate questa versione**: usate la 5.2.

## Funzionamento

| Presa | Sessione nel nodo RFID | Uscita | Tessere | Credito |
|---|---|---|---|---|
| Tipo 2 (AUTO) | slot 0 | contattore della wallbox (PC3), come prima | tipo 1 | kWh |
| SCHUKO 1 | slot 2 (**nuovo**) | **D2** | tipo 3 | minuti |
| SCHUKO 2 | slot 1 | **D5** | tipo 3 | minuti |
| (D4) | – | sempre spenta | – | – |

- **Tessera Auto:** parte senza conferma, come prima. Se l'AUTO è già occupata compare "Ricarica gia' in corso".
- **Tessera Schuko:** compare la pagina CONFERMA SELEZIONE e l'utente sceglie SCHUKO 1 o SCHUKO 2.
  Se la presa scelta è occupata compare "Ricarica gia' in corso".
  Se sono occupate entrambe, il messaggio compare subito.
  Senza scelta per 30 s, la richiesta viene annullata.
- **Vecchie tessere Scame (tipo 2):** "Operazione non disponibile". Vanno riprogrammate.
- **Ripassare la tessera** su una presa attiva chiude la sessione e riscrive il credito residuo sulla tessera.
- **Dopo un blackout** tornano anche le sessioni SCHUKO 1 e compare la pagina "riprendi".
  - **SÌ:** riprendono tutte e tre le sessioni.
  - **NO:** si fermano tutte e tre.
- **Emergenza (ingresso D6) o allarme della wallbox:** si fermano tutte e tre le sessioni.

Il programmatore RFID (rfid_bridge) **non va modificato**: Auto = tipo 1 (kWh), Schuko = tipo 3 (minuti).

## Cablaggio
È lo stesso della versione 5.2:
- **SCHUKO 1** (nuova presa al posto della Scame) sul relè comandato da **D2**, con una linea propria e protetta.
- **SCHUKO 2** (presa Schuko esistente) su **D5**.
- **D4** non è più usata.
- La Tipo 2 resta collegata direttamente alla wallbox.

Fate verificare il cablaggio da un elettricista.

## Programmazione
1. **WALLBOX (STM32):**
   - caricare `WALLBOX_STM32_v5.3_...hex` con ST-Link, **senza "Full chip erase"**: la pagina 0x0800FC00
     con impostazioni, statistiche e seriale va preservata;
   - nella pagina statistiche la versione diventa **5.4**.
2. **NODO RFID (ATmega328P a 8 MHz):**
   - caricare `NODO_RFID_ATmega328P_3PRESE_...hex` con il metodo usato finora;
   - lo stato delle sessioni salvato in EEPROM resta valido: il nuovo slot usa un'area libera
     (indirizzi EEPROM 0x260–0x2AF).
3. **Aggiornare sempre WALLBOX e NODO RFID insieme.** Il formato del messaggio di stato è cambiato
   (da 15 a 19 byte): una versione nuova con una vecchia non comunica.
   Aggiornare **senza sessioni attive o sospese**.
4. **Display:** stessa modifica della 5.2. Nel Nextion Editor sostituire l'immagine **ID 3** con
   `NEXTION_Immagine_ID3_..._800x480.png`, ricompilare e caricare il .tft.
   Consigliato: aggiungere nella pagina `conferma` un'area touch (Hotspot) sopra INDIETRO
   (x=14, y=393, w=167, h=70) con il comando `printh 71 51 00 00 00 FF FF FF`.

## Prove in simulazione (nodo RFID)
Il firmware RFID modificato è stato eseguito nel simulatore AVR `simavr`. Il banco di prova emula
la seriale verso la wallbox, un lettore MFRC522 senza tessera, l'EEPROM e i pin.
La lettura fisica della tessera non è emulata: lo stato "tessera letta" viene impostato direttamente in memoria.
Risultati (`06_TEST_SIMULATORE/TEST_risultati_nodo_RFID.txt`):

| Prova | Esito |
|---|---|
| Schuko → SCHUKO 1: D2 acceso, stato e UID corretti | ✅ |
| Schuko → SCHUKO 2: D5 acceso | ✅ |
| Auto mentre le due Schuko sono attive: tre sessioni contemporanee | ✅ |
| SCHUKO 1 occupata: messaggio di errore 2, nessuna modifica | ✅ |
| Annullamento (INDIETRO o timeout) | ✅ |
| Tessera Schuko senza scelta valida: nessuna presa attivata | ✅ |
| Credito a tempo che scala di 1 minuto, salvataggio EEPROM del nuovo slot | ✅ |
| Riavvio: ripristino da EEPROM, relè spenti fino a "riprendi", poi riaccesi | ✅ |
| Stop SCHUKO 1 dalla wallbox (comando 0x13 indice 2) | ✅ |
| Emergenza: tutte e tre le sessioni fermate, relè spenti | ✅ |

**Non** è stato possibile simulare la WALLBOX (STM32 + display) né la lettura reale delle tessere.
Il collaudo sulla colonnina è quindi indispensabile.

### Nota su un limite già presente nel progetto originale
Il nodo RFID comunica con la wallbox tramite una seriale software (pin D7/D8) che non riceve mentre trasmette.
In simulazione questo ha corrotto ogni tanto un byte ricevuto, **con il firmware originale come con quello modificato**.
Se viene corrotto il valore di energia inviato dalla wallbox, una sessione AUTO può chiudersi in anticipo.
Il credito non va perso: resta in memoria e torna sulla tessera ripassandola.
Il messaggio di stato ora è di 22 byte invece di 18, quindi la trasmissione dura circa il 20% in più.
Se in campo notate chiusure anomale delle ricariche Auto, segnalatelo.

## Modifiche in dettaglio

### Nodo RFID
- Terzo slot di sessione nella RAM a 0x02D1. Le variabili che occupavano quella zona (52 riferimenti a timer)
  sono state spostate a 0x03D0, e l'azzeramento della RAM all'avvio è stato esteso fino a 0x0400.
- Nuovo codice a 0x3A00–0x3CFB:
  - comando delle uscite;
  - messaggio di stato a 19 byte (bit0 = Auto, bit1 = SCHUKO 1, bit2 = SCHUKO 2, bit3 = emergenza;
    minuti di SCHUKO 1 e 2; UID delle tre sessioni);
  - scelta della presa alla conferma;
  - riconoscimento della tessera già attiva su SCHUKO 1;
  - stop di emergenza e LED estesi a tre sessioni;
  - salvataggio e ripristino del terzo slot in EEPROM.
- Il controllo dei crediti scorre ora 3 slot. Il comando di stop 0x13 accetta l'indice 2.

### WALLBOX
- Il messaggio di stato tipo 1 passa da 15 a 19 byte. L'UID di SCHUKO 1 viene salvato in RAM a 0x20004000.
- Pagina tempi: AUTO = bit0, SCHUKO 1 = bit1, ciascuna con il proprio UID; SCHUKO 1 mostrata in minuti.
- Il contattore della Tipo 2 non viene mai bloccato dalle Schuko: il vecchio flag "Scame" è forzato a 0.
- Tessera Schuko: pagina `conferma` con timeout di 30 s (logica già presente nel firmware, ora riattivata).
- Ripresa dopo blackout, risposta NO: ferma anche lo slot 2; il popup mostra l'UID corretto.
- Messaggio "Ricarica gia' in corso" generico; versione 5.4.
- (5.4) Popup "minuti esauriti" della SCHUKO 1 con l'UID della SCHUKO 1 invece che con quello della card AUTO.

## Collaudo sulla colonnina (obbligatorio)
- [ ] Le tre prese attive insieme, con tre tessere diverse: relè D2 e D5 chiusi, veicolo in carica, display corretto.
- [ ] Pagina "CREDITO CARTA RFID": UID e valori giusti sotto AUTO, SCHUKO 1 e SCHUKO 2.
- [ ] Ripassare ciascuna tessera: chiude solo la sua presa e il credito residuo torna sulla tessera.
- [ ] (5.4) Con l'AUTO attiva, chiudere più volte la SCHUKO 1 con la sua card: non deve più comparire il popup con l'UID della card AUTO.
- [ ] Credito Schuko esaurito: la presa si spegne da sola.
- [ ] Credito Auto esaurito: la ricarica si ferma.
- [ ] Scelta di una presa occupata: messaggio "gia' in corso".
- [ ] Togliere corrente durante tre ricariche: al ritorno compare "riprendi"; provare sia SÌ sia NO.
- [ ] Emergenza: tutto spento.
- [ ] Vecchia tessera Scame: "Operazione non disponibile".
- [ ] Misurare la corrente totale con tutte e tre le prese a pieno carico e confrontarla con le protezioni.
