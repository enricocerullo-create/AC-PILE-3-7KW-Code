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
static int errcode=-1; static int nframes=0;
static void uart_out(struct avr_irq_t*irq,uint32_t v,void*p){
  uint8_t b=v; if(rxn==0 && b!=0xAA) return; rx[rxn++]=b;
  if(rxn>=2){ int need = rx[1]==1?22: rx[1]==2?12: rx[1]==3?4: rx[1]==4?20: 4;
    if(rxn>=need){ if(rx[need-1]!=0x55) printf("!! frame tipo %d senza 0x55\n",rx[1]);
      if(rx[1]==1){memcpy(last1,rx,22);have1=1;nframes++; rx_kick();} if(rx[1]==3){errcode=rx[2]; printf("   <- errore RFID codice %d\n",rx[2]);}
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
/* ---------- SPI: MFRC522 minimale (nessuna tessera) ---------- */
static uint8_t reg[64]; static int spi_state=0, spi_addr=0, spi_read=0;
static void spi_out(struct avr_irq_t*irq,uint32_t v,void*p){
  uint8_t b=v, reply=0;
  if(spi_state==0){ spi_read=b&0x80; spi_addr=(b>>1)&0x3f; spi_state=1; reply=0; }
  else { if(spi_read){ reply=reg[spi_addr]; if(spi_addr==0x04) reply=0x01; if(spi_addr==0x37) reply=0x92;
           /* letture consecutive: il byte inviato e' il prossimo indirizzo */
           spi_addr=(b>>1)&0x3f; spi_read=b&0x80; }
         else { reg[spi_addr]=b; if(spi_addr==0x01 && (b&0x0f)==0x0f) reg[0x01]=0; spi_state=0; } }
  avr_raise_irq(avr_io_getirq(avr,AVR_IOCTL_SPI_GETIRQ(0),SPI_IRQ_INPUT),reply);
}
/* SS (D10) alto -> fine transazione */
static void ss_cb(struct avr_irq_t*irq,uint32_t v,void*p){ if(v) spi_state=0; }
/* ---------- pin relè ---------- */
static int pin[8];
static void pin_cb(struct avr_irq_t*irq,uint32_t v,void*p){ pin[(intptr_t)p]=v; }
/* ---------- utilità ---------- */
static uint32_t millis(){ return avr->data[0x2EE]|avr->data[0x2EF]<<8|avr->data[0x2F0]<<16|(uint32_t)avr->data[0x2F1]<<24; }
static void run_ms(double ms){ avr_cycle_count_t end=avr->cycle+(avr_cycle_count_t)(ms*8000); while(avr->cycle<end){ if(avr->pc==0x1b98){ uint16_t sp=avr->data[0x5d]|avr->data[0x5e]<<8; printf("   STOP slot %d da %04x E=%02x%02x%02x%02x\n",avr->data[24],((avr->data[sp+1]<<8)|avr->data[sp+2])*2,avr->data[0x1FC],avr->data[0x1FB],avr->data[0x1FA],avr->data[0x1F9]); } int s=avr_run(avr); if(s==cpu_Done||s==cpu_Crashed){printf("CPU stato %d!\n",s);exit(1);} } }
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
int main(int argc,char**argv){ if(argc>2) RXBIT=atoi(argv[2]);
  memset(ee,0xFF,sizeof ee);
  printf("== T1 avvio a EEPROM vuota\n"); boot(argv[1]); run_s(3,0); status("T1");
  printf("== T2 tessera Schuko (30 min) -> scelta SCHUKO 1\n"); pending(3,0,30,0x11); confirm(1,1); run_s(2,0); status("T2");
  printf("== T3 tessera Schuko (45 min) -> scelta SCHUKO 2\n"); pending(3,0,45,0x12); confirm(1,2); run_s(2,0); status("T3");
  printf("== T4 tessera Auto (credito 1234) -> conferma automatica (scelta 0)\n"); pending(1,1,1234,0x13); confirm(1,0); run_s(2,1); status("T4");
  printf("== T5 altra tessera Schuko -> SCHUKO 1 occupata (atteso errore 2)\n"); errcode=-1; pending(3,0,20,0x14); confirm(1,1); run_s(2,1); status("T5"); printf("   errore ricevuto=%d\n",errcode);
  printf("== T6 annullamento (ok=0)\n"); pending(3,0,20,0x15); confirm(0,0); run_s(1,1); printf("   pending=%d\n",avr->data[0x273]);
  printf("== T7 tessera Schuko con scelta 0 (non valida) -> nessuna presa\n"); pending(3,0,20,0x16); confirm(1,0); run_s(1,1); status("T7");
  printf("== T8 attesa 65 s (scalo minuti + salvataggio EEPROM)\n"); run_s(65,1); status("T8");
  save_ee(); { int idx=(avr->data[0x174]-1)&3; uint8_t*r=ee+0x260+20*idx; printf("   record EEPROM slot C idx %d: ",idx); for(int i=0;i<20;i++)printf("%02X ",r[i]); printf("\n"); }
  printf("== T9 riavvio (blackout) con EEPROM salvata\n"); boot(argv[1]); run_s(3,0); status("T9");
  printf("== T10 wallbox: riprendi (cmd 0x14)\n"); { uint8_t c[]={0xAA,0x14,0x55}; for(int k=0;k<4;k++){send(c,3); run_ms(300);} } run_s(2,1); status("T10");
  printf("== T11 wallbox: stop SCHUKO 1 (cmd 0x13 indice 2)\n"); { uint8_t c[]={0xAA,0x13,0x02,0x55}; for(int k=0;k<4;k++){send(c,4); run_ms(300);} } run_s(2,1); status("T11");
  printf("== T12 emergenza (D6 alto)\n"); emerg=1; run_s(3,1); status("T12");
  printf("frame di stato ricevuti: %d\n",nframes);
  return 0;
}
