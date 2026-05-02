#pragma once
/*
 * midifile.h  -  SMF (Standard MIDI File) Player + Recorder
 *
 * Player:
 *   Liest .mid-Dateien von SD-Karte (/midi/*.mid).
 *   Unterstuetzt Format 0 (single track) und Format 1 (multi track, merged).
 *   Tempoaenderungen (Set Tempo Meta-Event) werden beachtet.
 *   Alle MIDI-Events werden ueber die routeMsg()-Callback an den AI20 geschickt.
 *   SysEx-Events werden uebergeben.
 *   Meta-Events (Tempo, Track Name) werden geparst, der Rest ignoriert.
 *
 * Recorder:
 *   Zeichnet eingehende MIDI-Nachrichten mit Timestamp auf.
 *   Schreibt Format-0 SMF auf SD-Karte.
 *   Max. Aufnahmedauer: begrenzt durch RAM-Buffer (MAX_REC_EVENTS Events).
 *   Flush auf SD beim Stop.
 *
 * Callback-Typ: void(uint8_t status, uint8_t d1, uint8_t d2)
 */

#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>

typedef void (*MidiOutCallback)(uint8_t, uint8_t, uint8_t);

// ── Konstanten ────────────────────────────────────────────────────────────────
#define MIDI_DIR        "/midi"
#define MAX_TRACKS      32
#define MAX_REC_EVENTS  8192   // ~8k Events im RAM-Buffer

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
static uint32_t readVarLen(File& f, uint32_t& consumed) {
  uint32_t val = 0; uint8_t b; consumed = 0;
  do { b = f.read(); consumed++; val = (val << 7) | (b & 0x7F); } while (b & 0x80);
  return val;
}
static uint32_t read32BE(File& f) {
  uint32_t v = 0;
  for(int i=0;i<4;i++) v=(v<<8)|f.read();
  return v;
}
static uint16_t read16BE(File& f) {
  return ((uint16_t)f.read()<<8)|f.read();
}

// ── MIDI File Player ──────────────────────────────────────────────────────────
class MidiFilePlayer {
public:
  enum State { STOPPED, PLAYING, PAUSED };

  State    state      = STOPPED;
  char     filename[64] = "";
  uint32_t tempo      = 500000;  // us per beat (120 BPM default)
  uint16_t ppq        = 480;
  float    usPerTick  = 0;
  uint32_t tickPos    = 0;
  uint32_t lastUs     = 0;
  uint8_t  runStatus  = 0;

  // Track state (Format 1: alle Tracks zusammengefuehrt in einen Event-Stream)
  // Wir parsen alle Tracks vorab und mergen sie nach absolutem Tick.
  struct Event {
    uint32_t tick;
    uint8_t  status, d1, d2;
    bool     isSysEx;
    uint8_t  sxBuf[8];  // SysEx bis 8 Bytes inline (groessere werden ignoriert)
    uint8_t  sxLen;
  };

  Event*   events    = nullptr;
  uint32_t evCount   = 0;
  uint32_t evIdx     = 0;

  MidiOutCallback _cb = nullptr;

  void setCallback(MidiOutCallback cb) { _cb = cb; }

  bool load(const char* path) {
    stop();
    File f = SD_MMC.open(path);
    if(!f){ Serial.printf("[SMF] Cannot open: %s\n", path); return false; }

    // Header
    char hdr[5]={0}; f.read((uint8_t*)hdr,4);
    if(strncmp(hdr,"MThd",4)!=0){ f.close(); return false; }
    uint32_t hlen = read32BE(f);
    uint16_t fmt  = read16BE(f);
    uint16_t ntrk = read16BE(f);
    uint16_t div  = read16BE(f);
    f.seek(4+4+hlen); // skip any extra header bytes

    if(div & 0x8000){ Serial.println("[SMF] SMPTE timecode not supported"); f.close(); return false; }
    ppq = div;

    // Tempor. Event-Array im Heap (PSRAM verfuegbar auf N16R8)
    uint32_t maxEv = 65536;
    if(events) free(events);
    events  = (Event*)ps_malloc(maxEv * sizeof(Event));
    evCount = 0;

    if(fmt == 0) ntrk = 1;

    for(uint16_t t=0; t<ntrk && evCount<maxEv; t++) {
      // Track header
      char thdr[5]={0}; f.read((uint8_t*)thdr,4);
      if(strncmp(thdr,"MTrk",4)!=0){ f.close(); return false; }
      uint32_t tlen = read32BE(f);
      uint32_t tEnd = f.position() + tlen;

      uint32_t absTick = 0;
      uint8_t  rs      = 0;

      while(f.position() < tEnd && evCount < maxEv) {
        uint32_t c;
        uint32_t dt = readVarLen(f, c);
        absTick += dt;

        uint8_t b = f.read();

        if(b == 0xFF) {
          // Meta-event
          uint8_t mtype = f.read();
          uint32_t mlen = readVarLen(f, c);
          if(mtype == 0x51 && mlen == 3) {
            // Set Tempo - store as event with status=0xFF, d1=type
            uint32_t tp = ((uint32_t)f.read()<<16)|((uint32_t)f.read()<<8)|f.read();
            Event& ev = events[evCount++];
            ev.tick=absTick; ev.status=0xFF; ev.d1=0x51; ev.d2=0;
            ev.isSysEx=false;
            // encode tempo in sxBuf[0..2]
            ev.sxBuf[0]=(tp>>16)&0xFF; ev.sxBuf[1]=(tp>>8)&0xFF; ev.sxBuf[2]=tp&0xFF;
            ev.sxLen=3;
          } else {
            f.seek(f.position() + mlen);
          }
        } else if(b == 0xF0 || b == 0xF7) {
          // SysEx
          uint32_t slen = readVarLen(f, c);
          Event& ev = events[evCount++];
          ev.tick=absTick; ev.status=b; ev.d1=0; ev.d2=0; ev.isSysEx=true;
          ev.sxLen = (uint8_t)min((uint32_t)8, slen);
          for(uint8_t i=0;i<ev.sxLen;i++) ev.sxBuf[i]=f.read();
          if(slen > ev.sxLen) f.seek(f.position()+(slen-ev.sxLen));
        } else {
          // MIDI event
          uint8_t status, d1, d2=0;
          if(b & 0x80){ status=b; rs=b; d1=f.read(); }
          else         { status=rs; d1=b; }
          uint8_t cmd = status & 0xF0;
          bool need2 = !(cmd==0xC0||cmd==0xD0||cmd==0xF1||cmd==0xF3||status>=0xF4);
          if(need2) d2=f.read();

          Event& ev = events[evCount++];
          ev.tick=absTick; ev.status=status; ev.d1=d1; ev.d2=d2; ev.isSysEx=false;
        }
      }
      f.seek(tEnd); // jump to end of track (in case of errors)
    }
    f.close();

    // Sort events by tick (insertion sort on small sets, merge for large)
    // For simplicity use stdlib qsort via lambda
    qsort(events, evCount, sizeof(Event), [](const void* a, const void* b) -> int {
      return (int)((const Event*)a)->tick - (int)((const Event*)b)->tick;
    });

    strncpy(filename, path, 63);
    evIdx   = 0;
    tickPos = 0;
    tempo   = 500000;
    updateUsPerTick();

    Serial.printf("[SMF] Loaded: %s  events=%lu  ppq=%d\n", path, evCount, ppq);
    return true;
  }

  void play() {
    if(evCount == 0) return;
    state   = PLAYING;
    lastUs  = micros();
    Serial.println("[SMF] Play");
  }

  void pause() {
    if(state==PLAYING) state=PAUSED;
    else if(state==PAUSED) { state=PLAYING; lastUs=micros(); }
  }

  void stop() {
    state  = STOPPED;
    evIdx  = 0;
    tickPos= 0;
    // All notes off on all channels
    if(_cb) for(uint8_t ch=0;ch<16;ch++) _cb(0xB0|ch,123,0);
  }

  void rewind() { evIdx=0; tickPos=0; tempo=500000; updateUsPerTick(); }

  // Returns 0.0 - 1.0 progress
  float progress() {
    if(evCount==0) return 0;
    return (float)evIdx / evCount;
  }

  uint32_t currentBPM() { return 60000000UL / tempo; }

  // Call every loop() - sends due events
  void update() {
    if(state != PLAYING || evCount == 0) return;

    uint32_t now = micros();
    uint32_t elapsed = now - lastUs;
    lastUs = now;

    // Advance tick position
    float ticks = elapsed / usPerTick;
    tickPos += (uint32_t)ticks;

    // Dispatch all events up to tickPos
    while(evIdx < evCount && events[evIdx].tick <= tickPos) {
      Event& ev = events[evIdx++];
      if(ev.status == 0xFF && ev.d1 == 0x51) {
        // Tempo change
        tempo = ((uint32_t)ev.sxBuf[0]<<16)|((uint32_t)ev.sxBuf[1]<<8)|ev.sxBuf[2];
        updateUsPerTick();
      } else if(ev.isSysEx) {
        // SysEx passthrough - only if callback supports it (simplified)
        // Skipped here; handled in main via direct MidiAI20.write if needed
      } else if(_cb) {
        _cb(ev.status, ev.d1, ev.d2);
      }
    }

    // End of file
    if(evIdx >= evCount) {
      stop();
      Serial.println("[SMF] End of file");
    }
  }

  // List .mid files on SD
  static uint8_t listFiles(char names[][64], uint8_t maxFiles) {
    uint8_t count = 0;
    File dir = SD_MMC.open(MIDI_DIR);
    if(!dir || !dir.isDirectory()) return 0;
    File f = dir.openNextFile();
    while(f && count < maxFiles) {
      if(!f.isDirectory()) {
        const char* name = f.name();
        size_t len = strlen(name);
        if(len>4 && strcasecmp(name+len-4,".mid")==0) {
          snprintf(names[count], 64, "%s/%s", MIDI_DIR, name);
          count++;
        }
      }
      f = dir.openNextFile();
    }
    return count;
  }

private:
  void updateUsPerTick() { usPerTick = (float)tempo / ppq; }
};

// ── MIDI Recorder ─────────────────────────────────────────────────────────────
class MidiRecorder {
public:
  enum State { IDLE, RECORDING };
  State    state    = IDLE;
  uint32_t startUs  = 0;
  uint16_t ppq      = 480;
  uint32_t tempo    = 500000;

  struct RecEvent {
    uint32_t us;    // micros() timestamp
    uint8_t  status, d1, d2;
  };
  RecEvent* buf   = nullptr;
  uint32_t  count = 0;

  void begin() {
    if(!buf) buf = (RecEvent*)ps_malloc(MAX_REC_EVENTS * sizeof(RecEvent));
  }

  void startRec() {
    if(!buf) begin();
    count   = 0;
    startUs = micros();
    state   = RECORDING;
    Serial.println("[REC] Recording started");
  }

  void record(uint8_t status, uint8_t d1, uint8_t d2) {
    if(state != RECORDING || count >= MAX_REC_EVENTS) return;
    buf[count++] = { micros(), status, d1, d2 };
  }

  bool stopAndSave(const char* path) {
    state = IDLE;
    if(count == 0) return false;

    File f = SD_MMC.open(path, FILE_WRITE);
    if(!f){ Serial.printf("[REC] Cannot write: %s\n", path); return false; }

    // MThd
    f.write((uint8_t*)"MThd", 4);
    writeU32BE(f, 6);
    writeU16BE(f, 0);   // Format 0
    writeU16BE(f, 1);   // 1 track
    writeU16BE(f, ppq);

    // MTrk placeholder
    uint32_t trkStart = f.position();
    f.write((uint8_t*)"MTrk", 4);
    writeU32BE(f, 0); // placeholder

    // Tempo meta event at tick 0
    writeVarLen(f, 0);
    f.write((uint8_t)0xFF); f.write((uint8_t)0x51); f.write((uint8_t)0x03);
    f.write((uint8_t)((tempo>>16)&0xFF));
    f.write((uint8_t)((tempo>>8)&0xFF));
    f.write((uint8_t)(tempo&0xFF));

    uint32_t lastUs = startUs;
    for(uint32_t i=0;i<count;i++){
      uint32_t deltaUs = buf[i].us - lastUs;
      lastUs = buf[i].us;
      uint32_t deltaTicks = (uint32_t)((float)deltaUs / ((float)tempo / ppq));
      writeVarLen(f, deltaTicks);
      f.write(buf[i].status);
      uint8_t cmd = buf[i].status & 0xF0;
      bool two = !(cmd==0xC0||cmd==0xD0);
      f.write(buf[i].d1);
      if(two) f.write(buf[i].d2);
    }

    // End of track meta
    writeVarLen(f, 0);
    f.write((uint8_t)0xFF); f.write((uint8_t)0x2F); f.write((uint8_t)0x00);

    // Patch track length
    uint32_t trkEnd  = f.position();
    uint32_t trkLen  = trkEnd - trkStart - 8;
    f.seek(trkStart + 4);
    writeU32BE(f, trkLen);

    f.close();
    Serial.printf("[REC] Saved: %s  events=%lu\n", path, count);
    return true;
  }

private:
  void writeU32BE(File& f, uint32_t v) {
    f.write((v>>24)&0xFF); f.write((v>>16)&0xFF);
    f.write((v>>8)&0xFF);  f.write(v&0xFF);
  }
  void writeU16BE(File& f, uint16_t v) {
    f.write((v>>8)&0xFF); f.write(v&0xFF);
  }
  void writeVarLen(File& f, uint32_t v) {
    uint8_t buf[4]; int len=0;
    do { buf[len++] = v & 0x7F; v >>= 7; } while(v);
    for(int i=len-1;i>=0;i--) f.write(buf[i] | (i?0x80:0x00));
  }
};

// ── SD-Karte Init ─────────────────────────────────────────────────────────────
bool sdBegin() {
  // Freenove S3-WROOM: SDMMC 1-bit, Pins GPIO38/39/40 (fest auf Board)
  if(!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    Serial.println("[SD] Mount failed");
    return false;
  }
  // Sicherstellen dass /midi-Ordner existiert
  if(!SD_MMC.exists(MIDI_DIR)) SD_MMC.mkdir(MIDI_DIR);
  Serial.printf("[SD] Mounted, size: %lluMB\n", SD_MMC.totalBytes()/(1024*1024));
  return true;
}
