/*
 * Korg AI20 Wavetable MIDI Expander  -  main.cpp v5
 *
 * Board:   Freenove ESP32-S3-WROOM N16R8 CAM (Kamera entfernt)
 * Display: ILI9341 320x240 SPI
 * Encoder: 3x bare (ohne Board), externe 10k Pull-ups an CLK+DT
 *
 * Encoder-Rollen:
 *   ENC1 (Nav):  Dreht = Cursor durch Parameter-Menü
 *                Drückt = Edit-Mode ein/aus für gewählten Parameter
 *   ENC2 (Prog): Dreht = Programm/Bank direkt ändern (kein Edit-Mode nötig)
 *                Drückt = MIDI-File Player: Play/Pause / Datei wechseln
 *   ENC3 (Vol):  Dreht = Lautstärke des aktiven Kanals direkt
 *                Drückt = beim Start: Device-Modus erzwingen
 *                         im Betrieb: Kanal wechseln (schnell)
 *
 * USB-Modus:
 *   ENC3 beim Boot gedrückt = USB Device (PC MIDI-Interface)
 *   sonst = USB Host (Akai MPK mini oder anderes USB-MIDI-Gerät)
 *
 * SD-Karte:
 *   /midi/*.mid  - MIDI-Files abspielen
 *   /midi/rec_NNN.mid - aufgezeichnete Files
 *
 * Includes:
 *   pins.h      - GPIO-Definitionen
 *   encoder.h   - 3x Encoder ISR + Poll
 *   sounddb.h   - AI20 Sound-Datenbank
 *   midifile.h  - SMF Player + Recorder + SD-Init
 *   usbmode.h   - USB Host/Device Switching
 *   wireless.h  - BLE-MIDI + WiFi RTP-MIDI
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <EEPROM.h>

#include "pins.h"
#include "encoder.h"
#include "sounddb.h"
#include "midifile.h"
#include "usbmode.h"
#include "wireless.h"

// ── Display ───────────────────────────────────────────────────────────────────
SPIClass fspi(FSPI);
Adafruit_ILI9341 tft(&fspi, PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST);

// ── MIDI UARTs ────────────────────────────────────────────────────────────────
HardwareSerial MidiDIN(2);
HardwareSerial MidiAI20(1);

// ── MIDI File ─────────────────────────────────────────────────────────────────
MidiFilePlayer smfPlayer;
MidiRecorder   smfRecorder;
bool     sdOk       = false;
char     midiFiles[32][64];
uint8_t  midiFileCount  = 0;
uint8_t  midiFileIdx    = 0;
bool     recActive  = false;

// ── Farben ────────────────────────────────────────────────────────────────────
#define C_BG     ILI9341_BLACK
#define C_TITLE  0x07FF
#define C_SEL    0x1926
#define C_EDIT   0xFD20
#define C_TEXT   ILI9341_WHITE
#define C_DIM    0x39E7
#define C_GREEN  ILI9341_GREEN
#define C_YELLOW ILI9341_YELLOW
#define C_ACCENT 0x051F
#define C_RED    ILI9341_RED
#define C_ORANGE 0xFC60

// ── Per-Kanal State ───────────────────────────────────────────────────────────
struct ChannelState {
  uint8_t cc0=0, cc32=0, pc=0;
  uint8_t vol=100, rev=40, cho=0, pan=64;
  uint8_t bankIdx=0;
  BankID  bank=BankID::GM;
  uint8_t pendCC0=0, pendCC32=0;
};
ChannelState chState[16];

// ── Edit-State ────────────────────────────────────────────────────────────────
struct EditState {
  uint8_t  ch=0;
  uint8_t  transpose=64;
  uint8_t  menuIdx=0;       // aktueller Cursor (ENC1)
  bool     editing=false;   // ENC1 gedrückt = Edit-Mode für menuIdx
  uint8_t  activity=0;      // bit0=DIN, bit1=USB, bit2=OUT, bit3=BLE/RTP
  bool     needFull=true;
};
EditState ed;

// Menü-Parameter (ENC1-Navigation)
enum MenuParam : uint8_t { M_CHAN=0, M_BANK, M_PAN, M_TRANS, M_REV, M_CHO };
static const uint8_t MENU_COUNT = 6;
// ENC2 = Prog/Bank direkt, ENC3 = Vol direkt -> nicht im Cursor-Menü

// ── MIDI Helpers ──────────────────────────────────────────────────────────────
void toAI20(uint8_t b0, uint8_t b1=0xFF, uint8_t b2=0xFF) {
  MidiAI20.write(b0);
  if(b1!=0xFF) MidiAI20.write(b1);
  if(b2!=0xFF) MidiAI20.write(b2);
  ed.activity|=0x04;
}
void toDIN(uint8_t b0, uint8_t b1=0xFF, uint8_t b2=0xFF) {
  MidiDIN.write(b0);
  if(b1!=0xFF) MidiDIN.write(b1);
  if(b2!=0xFF) MidiDIN.write(b2);
}

static BankID resolveBank(uint8_t c0,uint8_t c32){
  if(c0==62) return BankID::USR;
  if(c32==1) return BankID::VAR;
  return BankID::GM;
}
static uint8_t bankCount(BankID b){
  switch(b){case BankID::GM:return 128;case BankID::VAR:return 22;case BankID::USR:return 1;case BankID::DRM:return 4;}
  return 128;
}
void sendBankAndPC(uint8_t ch){
  ChannelState& c=chState[ch];
  BankID b=(ch==9)?BankID::DRM:c.bank;
  const SoundEntry* e=getSoundEntry(b,c.bankIdx);
  if(!e) return;
  toAI20(0xB0|ch,0x00,e->cc0); toAI20(0xB0|ch,0x20,e->cc32); toAI20(0xC0|ch,e->pc);
  toDIN (0xB0|ch,0x00,e->cc0); toDIN (0xB0|ch,0x20,e->cc32); toDIN (0xC0|ch,e->pc);
}
void sendCC(uint8_t ch,uint8_t cc,uint8_t val){
  toAI20(0xB0|ch,cc,val); toDIN(0xB0|ch,cc,val);
}
void resetAI20(){
  const uint8_t sx[]={0xF0,0x7E,0x7F,0x09,0x01,0xF7};
  for(uint8_t b:sx) MidiAI20.write(b);
  delay(60);
}

// ── Display ───────────────────────────────────────────────────────────────────
void drawCell(uint8_t col, uint8_t row, const char* lbl, const char* val,
              bool sel, bool edit) {
  const uint16_t W=78, H=48;
  uint16_t x=2+col*80, y=32+row*52;
  uint16_t bg = edit?C_EDIT:(sel?C_SEL:C_ACCENT);
  uint16_t fg = (edit||sel)?C_BG:C_TEXT;
  uint16_t lc = edit?C_BG:(sel?C_YELLOW:C_DIM);
  tft.fillRect(x,y,W,H,bg);
  tft.drawRect(x,y,W,H,sel?C_YELLOW:C_DIM);
  tft.setTextSize(1); tft.setTextColor(lc); tft.setCursor(x+4,y+4); tft.print(lbl);
  tft.setTextSize(2); tft.setTextColor(fg); tft.setCursor(x+4,y+18); tft.print(val);
}

void drawDirectCell(uint8_t col, uint8_t row, const char* lbl, const char* val,
                    uint16_t accentColor) {
  // Zellen fuer ENC2 (Prog) und ENC3 (Vol) - immer direkt editierbar, kein Cursor
  const uint16_t W=78, H=48;
  uint16_t x=2+col*80, y=32+row*52;
  tft.fillRect(x,y,W,H,C_ACCENT);
  tft.drawRect(x,y,W,H,accentColor);
  tft.setTextSize(1); tft.setTextColor(accentColor); tft.setCursor(x+4,y+4); tft.print(lbl);
  tft.setTextSize(2); tft.setTextColor(C_TEXT); tft.setCursor(x+4,y+18); tft.print(val);
}

void drawSoundBanner() {
  ChannelState& c=chState[ed.ch];
  char name[17];
  BankID b=(ed.ch==9)?BankID::DRM:c.bank;
  const char* p=getSoundName(b,c.bankIdx);
  if(p) strncpy(name,p,16); else strncpy(name,"---",16); name[16]='\0';
  const char* bLbl[]={"GM","VAR","USR","DRM"};

  tft.fillRect(0,136,320,38,C_ACCENT);
  tft.drawFastHLine(0,136,320,C_DIM);
  tft.drawFastHLine(0,173,320,C_DIM);

  // Bank-Tag
  tft.setTextSize(1); tft.setTextColor(C_YELLOW);
  tft.setCursor(4,140); tft.print(bLbl[(uint8_t)b]);
  tft.setTextColor(C_DIM);
  if(ed.ch==9) tft.printf("  K%d",c.bankIdx+1);
  else         tft.printf("  #%d",c.bankIdx+1);

  // Sound-Name gross
  tft.setTextSize(3); tft.setTextColor(C_TEXT);
  tft.setCursor(4,152); tft.print(name);
}

void drawPlayerBar() {
  // MIDI-Player Status (y=176 - y=196)
  tft.fillRect(0,176,320,22,C_BG);
  tft.drawFastHLine(0,176,320,C_DIM);

  tft.setTextSize(1);

  if(!sdOk) {
    tft.setTextColor(C_RED);
    tft.setCursor(4,183); tft.print("SD: not found");
    return;
  }

  const char* stateIcons[] = {"[]", ">", "||"};
  uint8_t si = (uint8_t)smfPlayer.state;
  tft.setTextColor(smfPlayer.state==MidiFilePlayer::PLAYING ? C_GREEN : C_DIM);
  tft.setCursor(4,183); tft.print(stateIcons[si]);

  // Dateiname (ohne Pfad)
  tft.setTextColor(C_TEXT);
  const char* fname = midiFileCount>0 ? strrchr(midiFiles[midiFileIdx],'/') : nullptr;
  tft.setCursor(20,183);
  if(fname) tft.print(fname+1); else tft.print(midiFileCount>0?"no .mid files":"(no files)");

  // Fortschrittsbalken
  if(smfPlayer.state != MidiFilePlayer::STOPPED) {
    uint16_t barW = (uint16_t)(smfPlayer.progress() * 180);
    tft.fillRect(136,183,180,8,C_DIM);
    tft.fillRect(136,183,barW,8,C_GREEN);
    tft.setTextColor(C_DIM);
    tft.setCursor(320-28,183);
    tft.printf("%3d", smfPlayer.currentBPM());
  }

  // Rec-Indikator
  if(recActive) {
    tft.fillCircle(314,185,4,C_RED);
  }
}

void drawStatusBar() {
  tft.fillRect(0,200,320,40,C_BG);
  tft.drawFastHLine(0,200,320,C_DIM);
  tft.setTextSize(1);

  // MIDI-Aktivität
  struct{ uint8_t bit; const char* l; uint16_t x; } ind[]={
    {0x01,"DIN",4},{0x02,"USB",46},{0x04,"OUT",88},{0x08,"BT/RTP",130}
  };
  for(auto& m:ind){
    bool a=ed.activity&m.bit;
    tft.fillCircle(m.x+3,208,3,a?C_GREEN:C_DIM);
    tft.setTextColor(a?C_GREEN:C_DIM);
    tft.setCursor(m.x+8,204); tft.print(m.l);
  }

  // USB-Modus
  tft.setTextColor(isDeviceMode()?C_ORANGE:C_DIM);
  tft.setCursor(210,204); tft.print("USB:"); tft.print(usbModeLabel());

  // Wireless-Status
  bool ble=bleConnected(), rtp=rtpConnected(), wfi=wifiUp();
  tft.fillCircle(4,228,3, ble?C_GREEN:C_DIM);
  tft.setTextColor(ble?C_GREEN:C_DIM); tft.setCursor(10,224); tft.print("BLE");
  tft.fillCircle(42,228,3, wfi?C_GREEN:C_DIM);
  tft.setTextColor(wfi?C_GREEN:C_DIM); tft.setCursor(48,224);
  tft.print(wifiMode()==WifiMode::AP?"AP":"STA");
  tft.fillCircle(82,228,3, rtp?C_GREEN:C_DIM);
  tft.setTextColor(rtp?C_GREEN:C_DIM); tft.setCursor(88,224); tft.print("RTP");
  tft.setTextColor(wfi?0x7BEF:C_DIM); tft.setCursor(120,224); tft.print(wifiIP());

  ed.activity=0;
}

void drawUI(bool full=false) {
  if(full||ed.needFull) {
    tft.fillScreen(C_BG);
    tft.fillRect(0,0,320,30,C_ACCENT);
    tft.setTextSize(1); tft.setTextColor(C_TITLE);
    tft.setCursor(4,4); tft.print("KORG AI20 WAVETABLE EXPANDER");
    tft.setTextColor(C_DIM); tft.setCursor(262,4); tft.print("v5.0");
    tft.setTextColor(C_YELLOW); tft.setCursor(4,17);
    tft.printf("CH%02d", ed.ch+1);
    if(ed.ch==9){ tft.setTextColor(C_DIM); tft.print(" [DRUMS]"); }
    // Encoder-Legende
    tft.setTextColor(C_DIM); tft.setCursor(180,17);
    tft.print("E1:Nav E2:Prg E3:Vol");
    ed.needFull=false;
  }

  ChannelState& c=chState[ed.ch];
  char val[12];
  auto s=[&](MenuParam p)->bool{return ed.menuIdx==(uint8_t)p;};
  auto e=[&](MenuParam p)->bool{return s(p)&&ed.editing;};

  // Zeile 0: [ENC2->PROG] [ENC2->BANK] | [ENC1->CHAN] [ENC1->PAN]
  // ENC2-Zellen (direkter Zugriff, spezieller Rand)
  if(ed.ch==9) snprintf(val,sizeof(val),"K%d",c.bankIdx+1);
  else         snprintf(val,sizeof(val),"%3d",c.bankIdx+1);
  drawDirectCell(0,0,"PROG [E2]",val,C_ORANGE);

  if(ed.ch==9) strncpy(val,"DRM",sizeof(val));
  else { const char* bn[]={"GM","VAR","USR"}; strncpy(val,bn[(uint8_t)c.bank],sizeof(val)); }
  drawDirectCell(1,0,"BANK [E2]",val,C_ORANGE);

  // ENC1-Zellen (Cursor-Navigation)
  snprintf(val,sizeof(val),"%02d",ed.ch+1);
  drawCell(2,0,"CH [E1]",val,s(M_CHAN),e(M_CHAN));

  snprintf(val,sizeof(val),"%3d",c.pan);
  drawCell(3,0,"PAN",val,s(M_PAN),e(M_PAN));

  // Zeile 1: [ENC3->VOL] | [TRANS] [REV] [CHO]
  snprintf(val,sizeof(val),"%3d",c.vol);
  drawDirectCell(0,1,"VOL [E3]",val,0x07FF);

  snprintf(val,sizeof(val),"%+d",(int)ed.transpose-64);
  drawCell(1,1,"TRANS",val,s(M_TRANS),e(M_TRANS));

  snprintf(val,sizeof(val),"%3d",c.rev);
  drawCell(2,1,"REV",val,s(M_REV),e(M_REV));

  snprintf(val,sizeof(val),"%3d",c.cho);
  drawCell(3,1,"CHO",val,s(M_CHO),e(M_CHO));

  drawSoundBanner();
  drawPlayerBar();
  drawStatusBar();
}

// ── MIDI Routing ──────────────────────────────────────────────────────────────
void routeMsg(uint8_t status, uint8_t d1, uint8_t d2,
              bool trackState=true, bool dinThru=true) {
  uint8_t cmd=status&0xF0, ch=status&0x0F;

  // Transpose
  if(cmd==0x80||cmd==0x90){
    int n=(int)d1+((int)ed.transpose-64);
    d1=(uint8_t)constrain(n,0,127);
  }

  // State tracking
  if(trackState){
    if(cmd==0xB0){
      if(d1==0)   chState[ch].pendCC0=d2;
      if(d1==32)  chState[ch].pendCC32=d2;
      if(d1==7)   chState[ch].vol=d2;
      if(d1==10)  chState[ch].pan=d2;
      if(d1==91)  chState[ch].rev=d2;
      if(d1==93)  chState[ch].cho=d2;
    }
    if(cmd==0xC0&&ch!=9){
      chState[ch].bank=resolveBank(chState[ch].pendCC0,chState[ch].pendCC32);
      chState[ch].pc=d1;
      uint8_t cnt=bankCount(chState[ch].bank);
      for(uint8_t i=0;i<cnt;i++){
        const SoundEntry* ee=getSoundEntry(chState[ch].bank,i);
        if(ee&&ee->pc==d1){ chState[ch].bankIdx=i; break; }
      }
    }
  }

  // Recorder
  if(recActive) smfRecorder.record(status,d1,d2);

  toAI20(status,d1,d2);
  if(dinThru) toDIN(status,d1,d2);
  wirelessSendRaw(status,d1,d2);
  usbMidiSend(status,d1,d2);  // nur im Device-Modus aktiv
}

void onWirelessMidi(uint8_t s,uint8_t d1,uint8_t d2){ ed.activity|=0x08; routeMsg(s,d1,d2,true,true); }
void onUsbMidi(uint8_t s,uint8_t d1,uint8_t d2)     { ed.activity|=0x02; routeMsg(s,d1,d2,true,true); }

// SMF Player Callback
void onSmfMidi(uint8_t s,uint8_t d1,uint8_t d2){ routeMsg(s,d1,d2,false,true); }

// ── DIN MIDI Parser ───────────────────────────────────────────────────────────
uint8_t dinBuf[3],dinIdx=0,dinExp=0;
bool dinSX=false; uint8_t sxBuf[256],sxLen=0;

uint8_t midiMsgLen(uint8_t s){
  uint8_t c=s&0xF0;
  if(c==0xC0||c==0xD0) return 2;
  if(s>=0xF4)          return 1;
  if(s==0xF1||s==0xF3) return 2;
  return 3;
}
void processDIN(){
  while(MidiDIN.available()){
    uint8_t b=MidiDIN.read(); ed.activity|=0x01;
    if(dinSX){if(b==0xF7){sxBuf[sxLen++]=0xF7;for(uint16_t i=0;i<sxLen;i++) MidiAI20.write(sxBuf[i]);dinSX=false;sxLen=0;}else if(sxLen<255)sxBuf[sxLen++]=b;continue;}
    if(b==0xF0){dinSX=true;sxLen=0;sxBuf[sxLen++]=0xF0;continue;}
    if(b&0x80){dinBuf[0]=b;dinIdx=1;dinExp=midiMsgLen(b);if(dinExp==1){routeMsg(b,0,0);dinIdx=0;}}
    else if(dinIdx>0){dinBuf[dinIdx++]=b;if(dinIdx>=dinExp){routeMsg(dinBuf[0],dinBuf[1],dinExp>2?dinBuf[2]:0,true,false);dinIdx=0;}}
  }
}

// ── Encoder Handler ───────────────────────────────────────────────────────────
void handleEncoders() {
  ChannelState& c=chState[ed.ch];

  // ── ENC1: Navigation + Edit ───────────────────────────────────────────────
  if(encPressed(0)) { ed.editing=!ed.editing; drawUI(); }
  int d1=encDelta(0);
  if(d1) {
    if(!ed.editing) {
      ed.menuIdx=(ed.menuIdx+MENU_COUNT+(d1>0?1:MENU_COUNT-1))%MENU_COUNT;
    } else {
      switch((MenuParam)ed.menuIdx){
        case M_CHAN:  ed.ch=(ed.ch+16+d1)%16; ed.needFull=true; break;
        case M_PAN:   c.pan=(uint8_t)constrain((int)c.pan+d1,0,127);  sendCC(ed.ch,10,c.pan);  break;
        case M_TRANS: ed.transpose=(uint8_t)constrain((int)ed.transpose+d1,0,128); break;
        case M_REV:   c.rev=(uint8_t)constrain((int)c.rev+d1,0,127);  sendCC(ed.ch,91,c.rev);  break;
        case M_CHO:   c.cho=(uint8_t)constrain((int)c.cho+d1,0,127);  sendCC(ed.ch,93,c.cho);  break;
        default: break;
      }
    }
    drawUI(ed.needFull);
  }

  // ── ENC2: Programm/Bank + Player Control ─────────────────────────────────
  if(encPressed(1)) {
    // Button: Player Play/Pause oder Datei wechseln
    if(smfPlayer.state == MidiFilePlayer::PLAYING) {
      smfPlayer.pause();
    } else if(smfPlayer.state == MidiFilePlayer::PAUSED) {
      smfPlayer.pause(); // Resume
    } else {
      // Nächste Datei laden wenn STOPPED
      if(midiFileCount>0){
        midiFileIdx=(midiFileIdx+1)%midiFileCount;
        smfPlayer.load(midiFiles[midiFileIdx]);
        smfPlayer.play();
      }
    }
    drawUI();
  }
  int d2=encDelta(1);
  if(d2) {
    // Programm ändern (direkt, kein Edit-Mode)
    uint8_t mx=bankCount((ed.ch==9)?BankID::DRM:c.bank)-1;
    c.bankIdx=(uint8_t)constrain((int)c.bankIdx+d2,0,mx);
    if(ed.ch==9){ c.pc=c.bankIdx*8; toAI20(0xC0|9,c.pc); }
    else {
      const SoundEntry* e=getSoundEntry(c.bank,c.bankIdx);
      if(e){ c.pc=e->pc; sendBankAndPC(ed.ch); }
    }
    drawUI();
  }

  // ── ENC3: Volume + Kanal-Schnellwechsel ──────────────────────────────────
  if(encPressed(2)) {
    // Button: Kanal schnell weiterschalten
    ed.ch=(ed.ch+1)%16;
    ed.needFull=true;
    drawUI(true);
  }
  int d3=encDelta(2);
  if(d3) {
    c.vol=(uint8_t)constrain((int)c.vol+d3,0,127);
    sendCC(ed.ch,7,c.vol);
    drawUI();
  }
}

// ── EEPROM ────────────────────────────────────────────────────────────────────
#define EEPROM_MAGIC 0xA4
struct SavedState {
  uint8_t magic,editCh,transpose;
  struct{ uint8_t cc0,cc32,pc,vol,rev,cho,bankIdx,pan; } ch[16];
};
void saveState(){
  SavedState s; s.magic=EEPROM_MAGIC; s.editCh=ed.ch; s.transpose=ed.transpose;
  for(uint8_t i=0;i<16;i++) s.ch[i]={chState[i].cc0,chState[i].cc32,chState[i].pc,
    chState[i].vol,chState[i].rev,chState[i].cho,chState[i].bankIdx,chState[i].pan};
  EEPROM.put(0,s); EEPROM.commit();
}
void loadState(){
  SavedState s; EEPROM.get(0,s);
  if(s.magic!=EEPROM_MAGIC) return;
  ed.ch=s.editCh; ed.transpose=s.transpose;
  for(uint8_t i=0;i<16;i++){
    chState[i].cc0=s.ch[i].cc0;   chState[i].cc32=s.ch[i].cc32;
    chState[i].pc=s.ch[i].pc;     chState[i].vol=s.ch[i].vol;
    chState[i].rev=s.ch[i].rev;   chState[i].cho=s.ch[i].cho;
    chState[i].bankIdx=s.ch[i].bankIdx; chState[i].pan=s.ch[i].pan;
    chState[i].bank=resolveBank(s.ch[i].cc0,s.ch[i].cc32);
  }
}
void sendAllState(){
  for(uint8_t ch=0;ch<16;ch++){
    sendBankAndPC(ch); sendCC(ch,7,chState[ch].vol);
    sendCC(ch,10,chState[ch].pan); sendCC(ch,91,chState[ch].rev);
    sendCC(ch,93,chState[ch].cho); delay(5);
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup(){
  Serial.begin(115200);
  EEPROM.begin(sizeof(SavedState)+sizeof(WifiCreds)+16);

  MidiAI20.begin(31250,SERIAL_8N1,-1,PIN_MIDI_AI20_TX);
  MidiDIN.begin(31250,SERIAL_8N1,PIN_MIDI_DIN_RX,PIN_MIDI_DIN_TX);

  // Backlight
  ledcSetup(0,5000,8); ledcAttachPin(PIN_TFT_BL,0); ledcWrite(0,200);

  // Display
  fspi.begin(PIN_TFT_SCK,-1,PIN_TFT_MOSI,PIN_TFT_CS);
  tft.begin(40000000); tft.setRotation(1); tft.fillScreen(C_BG);

  // Splash
  tft.fillRect(0,0,320,240,C_BG);
  tft.setTextColor(C_TITLE); tft.setTextSize(3);
  tft.setCursor(20,60); tft.print("KORG AI20");
  tft.setTextColor(C_DIM); tft.setTextSize(1);
  tft.setCursor(20,100); tft.print("Wavetable MIDI Expander v5");
  tft.setCursor(20,114); tft.print("Freenove ESP32-S3 N16R8");

  // Encoders (vor USB-Mode-Init, da ENC3-Button dort gelesen wird)
  encodersBegin();

  // USB Mode (ENC3-Button beim Start = Device-Mode)
  tft.setCursor(20,130); tft.print("USB...");
  usbModeBegin(onUsbMidi);
  tft.setCursor(100,130); tft.setTextColor(isDeviceMode()?C_ORANGE:C_GREEN);
  tft.print(usbModeLabel());

  // SD-Karte
  tft.setTextColor(C_DIM); tft.setCursor(20,144); tft.print("SD...");
  sdOk = sdBegin();
  tft.setTextColor(sdOk?C_GREEN:C_RED);
  tft.setCursor(60,144); tft.print(sdOk?"OK":"FAIL");
  if(sdOk){
    midiFileCount = MidiFilePlayer::listFiles(midiFiles,32);
    tft.setTextColor(C_DIM);
    tft.setCursor(100,144); tft.printf("%d .mid files", midiFileCount);
    smfPlayer.setCallback(onSmfMidi);
    smfRecorder.begin();
  }

  // BLE + WiFi
  tft.setTextColor(C_DIM); tft.setCursor(20,158); tft.print("BLE+WiFi...");
  wirelessBegin(onWirelessMidi);
  tft.setCursor(20,172); tft.print("IP: "); tft.print(wifiIP());

  // AI20 init
  loadState();
  tft.setTextColor(C_DIM); tft.setCursor(20,186); tft.print("AI20 reset...");
  resetAI20(); delay(100); sendAllState();

  delay(500);
  ed.needFull=true; drawUI(true);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
static uint32_t lastSave=0, lastDraw=0;
static bool     dirty=false;

void loop(){
  handleEncoders();
  processDIN();
  usbModeUpdate();
  wirelessUpdate();
  smfPlayer.update();  // MIDI-File-Player tickt hier

  if(ed.activity && millis()-lastDraw>100){
    drawUI(ed.needFull); lastDraw=millis(); dirty=true;
  }
  if(dirty && millis()-lastSave>5000){
    saveState(); lastSave=millis(); dirty=false;
  }
}
