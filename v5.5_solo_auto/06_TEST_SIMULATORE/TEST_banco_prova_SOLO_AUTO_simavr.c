/* Banco di prova v5.5 SOLO AUTO - derivato da TEST_banco_prova_con_card_MIFARE_simavr.c (v5.4)
   gcc -O2 -o banco TEST_banco_prova_SOLO_AUTO_simavr.c -I/usr/include/simavr -lsimavr
   ./banco NODO_RFID_ATmega328P_v5.5_SOLO_AUTO_20261118.hex */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_hex.h>
#include <simavr/avr_spi.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_eeprom.h>
avr_t *avr;
/* ---------- UART: cattura frame dal nodo ---------- */
static void rx_kick(void);
static uint8_t rx[64]; static int rxn=0; static uint8_t last1[32]; static int have1=0;
static int errcode=-1; static int badA=0, lostA=0, watchA=0; static int nframes=0;
static void uart_out(struct avr_irq_t*irq,uint32_t v,void*p){
  uint8_t b=v; if(rxn==0 && b!=0xAA) return; rx[rxn++]=b;
  if(rxn>=2){ int need = rx[1]==1?22: rx[1]==2?12: rx[1]==3?4: rx[1]==4?20: 4;
    if(rxn>=need){ if(rx[need-1]!=0x55) printf("!! frame tipo %d senza 0x55\n",rx[1]);
      if(rx[1]==1){memcpy(last1,rx,22);have1=1;nframes++; if(!(rx[2]&1) && (rx[9]|rx[10]|rx[11]|rx[12])) { badA++; printf("   !!! frame con AUTO=0 ma UID auto presente (flags=%02X)\n",rx[2]); } if(watchA && !(rx[2]&1)) lostA++; rx_kick();} if(rx[1]==3){errcode=rx[2]; printf("   <- errore RFID codice %d\n",rx[2]);}
      rxn=0;} }
  if(rxn>40) rxn=0;
}
#define BIT 833
static int RXBIT=833;
static int txlvl=1, txbusy=0, txbit=0, txval=0; static avr_cycle_count_t lasttx=0;
static avr_cycle_count_t tx_sample(avr_t*a,avr_cycle_count_t when,void*p){
  if(txbit<8){ if(txlvl) txval|=1<<txbit; txbit++; return when+BIT; }
  if(!txlvl) printf("!! stop bit errato\n"); uart_out(0,txval,0); txbusy=0; return 0; }
static void tx_edge(struct avr_irq_t*irq,uint32_t v,void*p){ txlvl=v&1; lasttx=avr->cycle;
  if(!txbusy && !txlvl){ txbusy=1; txbit=0; txval=0; avr_cycle_timer_register(avr,BIT+BIT/2,tx_sample,0);} }
static uint8_t q[512]; static int qh=0,qt=0, rxbusy=0, rxbit=0; static uint8_t rxcur;
static avr_cycle_count_t rx_tick(avr_t*a,avr_cycle_count_t when,void*p){
  avr_irq_t*pin=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),7);
  if(rxbit==0){ if(qh==qt){ rxbusy=0; return 0;} if(txbusy || avr->cycle-lasttx<15*BIT) return when+BIT; rxcur=q[qh]; qh=(qh+1)%512; avr_raise_irq(pin,0); rxbit=1; return when+RXBIT; }
  if(rxbit<=8){ avr_raise_irq(pin,(rxcur>>(rxbit-1))&1); rxbit++; return when+RXBIT; }
  avr_raise_irq(pin,1); rxbit=0; return when+RXBIT*2; }
static void rx_kick(void){ if(!rxbusy && qh!=qt){ rxbusy=1; rxbit=0; avr_cycle_timer_register(avr,BIT*4,rx_tick,0);} }
static void send(const uint8_t*b,int n){ for(int k=0;k<n;k++){ q[qt]=b[k]; qt=(qt+1)%512; } }
/* ---------- SPI: emulazione MFRC522 + card MIFARE ---------- */
static uint8_t reg[64]; static int spi_state=0, spi_addr=0, spi_read=0;
static uint8_t fifo[64]; static int fifon=0, fifor=0, lastbits=0;
static const uint8_t KEY[16]={0xA5,0x3C,0x7B,0xE1,0x9F,0x42,0x08,0xD6,0x11,0x88,0xFE,0x23,0x6A,0x4D,0xC0,0x77};
typedef struct { int present, halted, wpend; uint8_t uid[4]; uint8_t blk[64][16]; int nwrites; } card_t;
static card_t card; static int cardlog=0;
static uint16_t crca(const uint8_t*d,int n){ uint16_t c=0x6363; for(int i=0;i<n;i++){ uint8_t b=d[i]; b^=c&0xff; b^=b<<4; c=(c>>8)^((uint16_t)b<<8)^((uint16_t)b<<3)^(b>>4);} return c; }
static uint16_t crc16(const uint8_t*d,int n){ uint16_t c=0xFFFF; for(int i=0;i<n;i++){ c^=(uint16_t)d[i]<<8; for(int k=0;k<8;k++) c=(c&0x8000)?(c<<1)^0x1021:(c<<1);} return c; }
static void card_make(const uint8_t uid[4],uint8_t type,uint32_t credit){
  memset(&card,0,sizeof card); memcpy(card.uid,uid,4);
  uint8_t b4[16]={0}; b4[0]=2; b4[1]=type; memcpy(b4+6,uid,4); uint16_t c=crc16(b4,14); b4[14]=c; b4[15]=c>>8;
  uint8_t b5[16]={0}; b5[0]=credit; b5[1]=credit>>8; b5[2]=credit>>16; b5[3]=credit>>24; c=crc16(b5,14); b5[14]=c; b5[15]=c>>8;
  for(int i=0;i<16;i++){ card.blk[4][i]=b4[i]^KEY[i]; card.blk[5][i]=b5[i]^KEY[i]; } }
static uint32_t card_credit(){ uint8_t b5[16]; for(int i=0;i<16;i++) b5[i]=card.blk[5][i]^KEY[i];
  uint16_t c=crc16(b5,14); if(b5[14]!=(c&0xff)||b5[15]!=(c>>8)) return 0xFFFFFFFF; return b5[0]|b5[1]<<8|b5[2]<<16|(uint32_t)b5[3]<<24; }
static void resp(const uint8_t*d,int n,int lb){ fifon=0; fifor=0; for(int i=0;i<n;i++) fifo[fifon++]=d[i]; lastbits=lb; reg[0x04]|=0x30; }
static void transceive(void){
  uint8_t *f=fifo; int n=fifon; int tx7=(reg[0x0D]&7)==7; uint8_t r[20];
  if(!card.present){ reg[0x04]|=0x01; fifon=fifor=0; return; }
  if(n==1 && tx7 && (f[0]==0x26||f[0]==0x52)){ if(card.halted && f[0]==0x26){reg[0x04]|=0x01; fifon=fifor=0; return;} card.halted=0; r[0]=0x04;r[1]=0; resp(r,2,0); return; }
  if(card.halted){ reg[0x04]|=0x01; fifon=fifor=0; return; }
  if(n>=2 && f[0]==0x93 && f[1]==0x20){ memcpy(r,card.uid,4); r[4]=card.uid[0]^card.uid[1]^card.uid[2]^card.uid[3]; resp(r,5,0); return; }
  if(n>=7 && f[0]==0x93 && f[1]==0x70){ r[0]=0x08; uint16_t c=crca(r,1); r[1]=c; r[2]=c>>8; resp(r,3,0); return; }
  if(n>=2 && f[0]==0x50){ card.halted=1; reg[0x04]|=0x01; fifon=fifor=0; return; }
  if(n==4 && f[0]==0x30){ memcpy(r,card.blk[f[1]&63],16); uint16_t c=crca(r,16); r[16]=c; r[17]=c>>8; resp(r,18,0); return; }
  if(n==4 && f[0]==0xA0){ card.wpend=f[1]|0x100; r[0]=0x0A; resp(r,1,4); return; }
  if(n==18 && card.wpend){ memcpy(card.blk[card.wpend&63],f,16); card.nwrites++; if(cardlog) printf("      [card] scritto blocco %d\n",card.wpend&63); card.wpend=0; r[0]=0x0A; resp(r,1,4); return; }
  if(cardlog) printf("      [card] comando sconosciuto %02X len %d\n",f[0],n);
  reg[0x04]|=0x01; fifon=fifor=0;
}
static void reg_write(int a,uint8_t v){
  switch(a){
  case 0x01: reg[1]=v&0x0f; if(v==0x0F) { memset(reg,0,sizeof reg); fifon=fifor=0; }
    else if((v&0x0f)==0x03){ uint16_t c=crca(fifo+fifor,fifon-fifor); reg[0x22]=c; reg[0x21]=c>>8; reg[0x05]|=0x04; }
    else if((v&0x0f)==0x0E){ reg[0x08]|=0x08; reg[0x04]|=0x10; }
    break;
  case 0x04: if(v&0x80) reg[4]|=v&0x7f; else reg[4]&=~(v&0x7f); break;
  case 0x05: if(v&0x80) reg[5]|=v&0x7f; else reg[5]&=~(v&0x7f); break;
  case 0x09: if(fifon<64) fifo[fifon++]=v; break;
  case 0x0A: if(v&0x80){ fifon=fifor=0; } break;
  case 0x0D: reg[0x0D]=v&0x7f; if((v&0x80) && reg[1]==0x0C) transceive(); break;
  default: reg[a]=v;
  } }
static uint8_t reg_read(int a){
  switch(a){
  case 0x09: return fifor<fifon? fifo[fifor++]:0;
  case 0x0A: return fifon-fifor;
  case 0x0C: return (reg[0x0C]&~7)|lastbits;
  case 0x06: return 0; case 0x0E: return 0x20;
  case 0x37: return 0x92;
  default: return reg[a]; } }
static void spi_out(struct avr_irq_t*irq,uint32_t v,void*p){
  uint8_t b=v, reply=0;
  if(spi_state==0){ spi_read=b&0x80; spi_addr=(b>>1)&0x3f; spi_state=1; }
  else if(spi_read){ reply=reg_read(spi_addr); spi_addr=(b>>1)&0x3f; }
  else reg_write(spi_addr,b);
  avr_raise_irq(avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_INPUT),reply);
}
static void ss_cb(struct avr_irq_t*irq,uint32_t v,void*p){ spi_state=0; }
/* ---------- pin relè ---------- */
static int pin[8];
static void pin_cb(struct avr_irq_t*irq,uint32_t v,void*p){ pin[(intptr_t)p]=v; }
/* ---------- utilità ---------- */
static uint32_t millis(){ return avr->data[0x2EE]|avr->data[0x2EF]<<8|avr->data[0x2F0]<<16|(uint32_t)avr->data[0x2F1]<<24; }
static void run_ms(double ms){ avr_cycle_count_t end=avr->cycle+(avr_cycle_count_t)(ms*8000); while(avr->cycle<end){ int s=avr_run(avr); if(s==cpu_Done||s==cpu_Crashed){printf("CPU stato %d!\n",s);exit(1);} } }
static uint32_t tlast=0;
static int emerg=0;
static void wallbox_tick(int flags){
  { avr_ioport_external_t e={.name='D',.mask=1<<6,.value=emerg?(1<<6):0}; avr_ioctl(avr,AVR_IOCTL_IOPORT_SET_EXTERNAL('D'),&e); }
  avr_raise_irq(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),6),emerg); /* modo 1 + heartbeat */
  uint8_t m[]={0xAA,0x21,0x01,0x55}; send(m,4); run_ms(30);
  uint8_t h[]={0xAA,0x20,flags,0,0,0,0,0x55}; send(h,8); }
static void run_s(double s,int flags){ for(double t=0;t<s;t+=0.5){ wallbox_tick(flags); run_ms(470);} }
static void status(const char*tag){
  printf("[%s] t=%.1fs D2(SCHUKO1)=%d D4=%d D5(SCHUKO2)=%d | ",tag,millis()/1000.0,pin[2],pin[4],pin[5]);
  if(have1){ uint8_t*f=last1; printf("flags=%02X a=%u b=%u c=%u uidA=%02X%02X%02X%02X uidB=%02X%02X%02X%02X uidC=%02X%02X%02X%02X\n",
    f[2],f[3]|f[4]<<8,f[5]|f[6]<<8,f[7]|f[8]<<8,f[9],f[10],f[11],f[12],f[13],f[14],f[15],f[16],f[17],f[18],f[19],f[20]);}
  else printf("(nessun frame stato)\n");
  printf("        RX ok=%d err=%d coda=%d\n",avr->data[0x1EB]|avr->data[0x1EC]<<8,avr->data[0x292]|avr->data[0x293]<<8,(qt-qh+512)%512); printf("        slotA act=%d tipo=%d cred=%u | slotB act=%d tipo=%d cred=%u | slotC act=%d tipo=%d cred=%u | recovery=%d\n",
    avr->data[0x2A1],avr->data[0x2A6],avr->data[0x2A8]|avr->data[0x2A9]<<8,
    avr->data[0x2B9],avr->data[0x2BE],avr->data[0x2C0]|avr->data[0x2C1]<<8,
    avr->data[0x2D1],avr->data[0x2D6],avr->data[0x2D8]|avr->data[0x2D9]<<8, avr->data[0x2E9]);
}
/* simula tessera letta e in attesa di conferma (stato che il firmware imposta dopo la lettura) */
static void pending(uint8_t type,uint8_t energy,uint32_t cred,uint8_t u0){
  uint32_t m=millis(); uint8_t*d=avr->data;
  d[0x273]=1; d[0x26E]=type; d[0x26D]=energy; d[0x269]=cred; d[0x26A]=cred>>8; d[0x26B]=cred>>16; d[0x26C]=cred>>24;
  d[0x26F]=u0; d[0x270]=0x22; d[0x271]=0x33; d[0x272]=0x44; d[0x265]=d[0x266]=d[0x267]=d[0x268]=0;
  d[0x261]=m; d[0x262]=m>>8; d[0x263]=m>>16; d[0x264]=m>>24; d[0x260]=0;
}
static void confirm(uint8_t ok,uint8_t choice){ uint8_t c[]={0xAA,0x22,ok,choice,0x55}; for(int k=0;k<12;k++){ send(c,5); run_ms(150);} }
static uint8_t ee[1024];
static void boot(const char*hex){
  avr=avr_make_mcu_by_name("atmega328p"); avr_init(avr); avr->frequency=8000000; avr->log=0;
  ihex_chunk_p ch; int n=read_ihex_chunks(hex,&ch); for(int i=0;i<n;i++) avr_loadcode(avr,ch[i].data,ch[i].size,ch[i].baseaddr);
  avr_eeprom_desc_t e={.ee=ee,.offset=0,.size=1024}; avr_ioctl(avr,AVR_IOCTL_EEPROM_SET,&e);
  avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('B'),0),tx_edge,0);
  avr_raise_irq(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),7),1); txlvl=1; txbusy=0; qh=qt=0; rxbusy=0;
  avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_OUTPUT),spi_out,0);
  avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('B'),2),ss_cb,0);
  for(intptr_t p=2;p<=5;p++) avr_irq_register_notify(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),p),pin_cb,(void*)p);
  avr_raise_irq(avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),6),0); /* emergenza non attiva */
  memset(pin,0,sizeof pin); have1=0; rxn=0; emerg=0;
}
static void save_ee(){ avr_eeprom_desc_t e={.ee=0,.offset=0,.size=1024}; avr_ioctl(avr,AVR_IOCTL_EEPROM_GET,&e); memcpy(ee,e.ee,1024); }
static void tap(const char*nome,double ms){ card.present=1; card.halted=0; int w=card.nwrites; run_s(ms/1000.0,0); card.present=0; card.halted=0; run_s(1,0);
  printf("   tap %s: scritture=%d credito sulla card=%u\n",nome,card.nwrites-w,card_credit()); }
/* ===== v5.5 SOLO AUTO: le Schuko devono restare sempre disattivate ===== */
static int fails=0;
static void check(const char*what,int ok){ printf("   %s %s\n",ok?"[OK]  ":"[FAIL]",what); if(!ok) fails++; }
static int schuko_off(){ return pin[2]==0 && pin[4]==0 && pin[5]==0 && avr->data[0x2B9]==0 && avr->data[0x2D1]==0 && (!have1 || (last1[2]&6)==0); }
int main(int argc,char**argv){
  memset(ee,0xFF,sizeof ee); boot(argv[1]); run_s(3,0);
  uint8_t uA[4]={0xD1,0xDE,0x2F,0x4B}, u1[4]={0x11,0x22,0x33,0x44}, u2[4]={0x55,0x66,0x77,0x88}, u3[4]={0x99,0xAA,0xBB,0xCC};
  printf("== 1 tessera AUTO (tipo 1, 500)\n"); card_make(uA,1,500); tap("AUTO",1500); confirm(1,0); run_s(2,0); status("1");
  card_t cA=card;
  check("AUTO attiva (slot 0, flag bit0, UID)", avr->data[0x2A1]==1 && have1 && (last1[2]&1) && last1[9]==0xD1);
  check("D2/D4/D5 spente", schuko_off());
  printf("== 2 tessera SCHUKO (tipo 3, 60 min) + scelta SCHUKO 1\n"); errcode=-1; card_make(u1,3,60); tap("S1",1500); confirm(1,1); run_s(2,0); status("2");
  check("errore 8 'Operazione non disponibile'", errcode==8);
  check("nessuna scrittura sulla card, credito invariato", card.nwrites==0 && card_credit()==60);
  check("SCHUKO 1/2 non attivate, uscite spente", schuko_off());
  check("AUTO ancora attiva", avr->data[0x2A1]==1 && (last1[2]&1));
  printf("== 3 tessera SCHUKO (tipo 3, 90 min) + scelta SCHUKO 2\n"); errcode=-1; card_make(u2,3,90); tap("S2",1500); confirm(1,2); run_s(2,0); status("3");
  check("errore 8", errcode==8); check("credito invariato", card.nwrites==0 && card_credit()==90); check("Schuko spente", schuko_off());
  printf("== 4 vecchia tessera SCAME (tipo 2)\n"); errcode=-1; card_make(u3,2,50); tap("SCAME",1500); run_s(2,0); status("4");
  check("errore 8", errcode==8); check("Schuko spente", schuko_off());
  printf("== 5 conferma forzata di una tessera tipo 3 (scelte 1 e 2) senza passare dalla lettura\n");
  pending(3,0,30,0x21); confirm(1,1); run_s(2,0); pending(3,0,30,0x22); confirm(1,2); run_s(2,0); status("5");
  check("nessuna Schuko avviata", schuko_off());
  printf("== 6 sessioni Schuko residue in RAM (es. da EEPROM vecchia): uscite comunque spente\n");
  avr->data[0x2B9]=1; avr->data[0x2C8]=0; avr->data[0x2D1]=1; avr->data[0x2E0]=0; run_s(2,0); status("6");
  check("D2 e D5 restano spente", pin[2]==0 && pin[5]==0);
  { uint8_t c[]={0xAA,0x13,0x01,0x55}; for(int k=0;k<4;k++){send(c,4); run_ms(300);} }
  { uint8_t c[]={0xAA,0x13,0x02,0x55}; for(int k=0;k<4;k++){send(c,4); run_ms(300);} } run_s(2,0); status("6b");
  check("slot residui chiusi con stop 0x13, AUTO intatta", schuko_off() && avr->data[0x2A1]==1);
  printf("== 7 sessione AUTO stabile per 20 s\n"); run_s(20,1); status("7");
  printf("== 8 ripasso tessera AUTO: chiusura e credito riscritto\n"); card=cA; tap("AUTO",600); run_s(2,0); status("8");
  check("AUTO chiusa", avr->data[0x2A1]==0 && have1 && !(last1[2]&1)); check("credito riscritto sulla card", card.nwrites>0);
  printf("== 9 nuova sessione AUTO dopo la chiusura\n"); card_make(u3,1,300); tap("AUTO2",1500); confirm(1,0); run_s(2,0); status("9");
  check("AUTO riparte", avr->data[0x2A1]==1 && (last1[2]&1)); check("Schuko spente", schuko_off());
  printf("== 10 emergenza\n"); emerg=1; run_s(3,0); status("10"); check("tutto fermo", avr->data[0x2A1]==0 && schuko_off());
  printf("\nRISULTATO: %d verifiche fallite, frame di stato %d\n",fails,nframes);
  return fails?1:0; }
