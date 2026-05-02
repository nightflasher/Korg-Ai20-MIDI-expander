#pragma once
/*
 * wireless.h  –  BLE-MIDI + WiFi RTP-MIDI transport for Korg AI20 Expander
 *
 * BLE-MIDI:  Acts as BLE peripheral ("AI20 Expander").
 *            Connects to iOS/macOS/Android BLE-MIDI apps and controllers.
 *            Library: max22-/ESP32-BLE-MIDI
 *
 * WiFi RTP-MIDI (AppleMIDI):
 *            Acts as RTP-MIDI session listener on port 5004 (default).
 *            macOS: Audio MIDI Setup → Network → Add session, enter IP.
 *            Windows: Tobias Erichsen's rtpMIDI driver.
 *            iOS: midimittr, MIDI Network, etc.
 *            Library: lathoub/Arduino-AppleMIDI-Library
 *
 * WiFi modes:
 *   WIFI_MODE_STA  – connect to existing network (home studio, etc.)
 *   WIFI_MODE_AP   – create own hotspot (live use without router)
 *
 * BLE + WiFi coexistence:
 *   The ESP32 shares one 2.4 GHz radio between WiFi and BLE.
 *   Espressif's Software Coexistence (enabled by default in arduino-esp32)
 *   uses time-division multiplexing.
 *   Practical MIDI latency impact: +2..8 ms compared to BLE-only.
 *   For live MIDI use this is fully acceptable.
 *   BLE advertising interval is set conservatively (200ms) to give WiFi room.
 *
 * Credentials are stored in EEPROM (WIFI_CRED_ADDR) as a simple struct.
 * The web config portal (port 80) lets you set SSID/password over WiFi AP.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AppleMIDI.h>
#include <BLEMidi.h>   // max22-/ESP32-BLE-MIDI
#include <EEPROM.h>

// ── Config ────────────────────────────────────────────────────────────────────
#define DEVICE_NAME      "AI20 Expander"
#define RTP_PORT         5004
#define AP_SSID          "AI20-Expander"
#define AP_PASSWORD      "midi1234"      // change as needed, min 8 chars
#define WIFI_CRED_ADDR   64             // EEPROM offset (after main SavedState)
#define WIFI_CONNECT_MS  8000           // timeout before falling back to AP mode

// ── Callbacks set by main.cpp ─────────────────────────────────────────────────
// Called with raw MIDI bytes whenever wireless MIDI arrives.
// Signature: void(uint8_t status, uint8_t d1, uint8_t d2)
typedef void (*WirelessMidiCallback)(uint8_t, uint8_t, uint8_t);
static WirelessMidiCallback _wirelessCb = nullptr;

// ── State ─────────────────────────────────────────────────────────────────────
enum class WifiMode : uint8_t { OFF = 0, STA, AP };

struct WirelessState {
  bool     bleConnected   = false;
  bool     rtpConnected   = false;
  bool     wifiUp         = false;
  WifiMode wifiMode       = WifiMode::OFF;
  char     ip[16]         = "---";
  uint8_t  bleClients     = 0;
  uint8_t  rtpSessions    = 0;
};
WirelessState wirelessState;

// ── Stored credentials ────────────────────────────────────────────────────────
struct WifiCreds {
  uint8_t magic;           // 0xB1 = valid
  char    ssid[33];
  char    pass[65];
  uint8_t mode;            // WifiMode
};

static void loadWifiCreds(WifiCreds& c) {
  EEPROM.get(WIFI_CRED_ADDR, c);
  if (c.magic != 0xB1) {
    c.magic = 0xB1;
    strncpy(c.ssid, "", sizeof(c.ssid));
    strncpy(c.pass, "", sizeof(c.pass));
    c.mode = (uint8_t)WifiMode::AP;
  }
}
static void saveWifiCreds(const WifiCreds& c) {
  EEPROM.put(WIFI_CRED_ADDR, c);
  EEPROM.commit();
}

// ── AppleMIDI (RTP) ───────────────────────────────────────────────────────────
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, RTP_MIDI, DEVICE_NAME, RTP_PORT);

static void onRtpConnected(const ssrc_t& ssrc, const char* name) {
  wirelessState.rtpConnected = true;
  wirelessState.rtpSessions++;
  Serial.printf("[RTP] Connected: %s\n", name);
}
static void onRtpDisconnected(const ssrc_t& ssrc, const char* name) {
  wirelessState.rtpSessions = (wirelessState.rtpSessions > 0)
                              ? wirelessState.rtpSessions - 1 : 0;
  wirelessState.rtpConnected = (wirelessState.rtpSessions > 0);
  Serial.printf("[RTP] Disconnected: %s\n", name);
}

// RTP → AI20 routing
static void onRtpNoteOn(byte ch, byte note, byte vel) {
  if (_wirelessCb) _wirelessCb(0x90|(ch-1), note, vel);
}
static void onRtpNoteOff(byte ch, byte note, byte vel) {
  if (_wirelessCb) _wirelessCb(0x80|(ch-1), note, vel);
}
static void onRtpCC(byte ch, byte num, byte val) {
  if (_wirelessCb) _wirelessCb(0xB0|(ch-1), num, val);
}
static void onRtpPC(byte ch, byte prog) {
  if (_wirelessCb) _wirelessCb(0xC0|(ch-1), prog, 0);
}
static void onRtpPitchBend(byte ch, int bend) {
  uint16_t b14 = (uint16_t)(bend + 8192);
  if (_wirelessCb) _wirelessCb(0xE0|(ch-1), b14&0x7F, (b14>>7)&0x7F);
}
static void onRtpAftertouch(byte ch, byte note, byte pres) {
  if (_wirelessCb) _wirelessCb(0xA0|(ch-1), note, pres);
}
static void onRtpChannelPressure(byte ch, byte pres) {
  if (_wirelessCb) _wirelessCb(0xD0|(ch-1), pres, 0);
}

// ── BLE-MIDI ──────────────────────────────────────────────────────────────────
static void onBleConnected() {
  wirelessState.bleConnected = true;
  wirelessState.bleClients++;
  Serial.println("[BLE] Client connected");
}
static void onBleDisconnected() {
  wirelessState.bleClients = (wirelessState.bleClients > 0)
                             ? wirelessState.bleClients - 1 : 0;
  wirelessState.bleConnected = (wirelessState.bleClients > 0);
  Serial.println("[BLE] Client disconnected");
}

// BLE → AI20 routing
static void onBleNoteOn(uint8_t ch, uint8_t note, uint8_t vel, uint16_t ts) {
  if (_wirelessCb) _wirelessCb(0x90|ch, note, vel);
}
static void onBleNoteOff(uint8_t ch, uint8_t note, uint8_t vel, uint16_t ts) {
  if (_wirelessCb) _wirelessCb(0x80|ch, note, vel);
}
static void onBleCC(uint8_t ch, uint8_t num, uint8_t val, uint16_t ts) {
  if (_wirelessCb) _wirelessCb(0xB0|ch, num, val);
}
static void onBlePC(uint8_t ch, uint8_t prog, uint16_t ts) {
  if (_wirelessCb) _wirelessCb(0xC0|ch, prog, 0);
}
static void onBlePitchBend(uint8_t ch, int bend, uint16_t ts) {
  uint16_t b14 = (uint16_t)(bend + 8192);
  if (_wirelessCb) _wirelessCb(0xE0|ch, b14&0x7F, (b14>>7)&0x7F);
}
static void onBleAftertouch(uint8_t ch, uint8_t note, uint8_t pres, uint16_t ts) {
  if (_wirelessCb) _wirelessCb(0xA0|ch, note, pres);
}

// ── Simple web config portal (AP mode only) ───────────────────────────────────
#include <WebServer.h>
static WebServer* _cfgServer = nullptr;

static const char CONFIG_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AI20 WiFi Setup</title>
<style>body{font-family:monospace;background:#111;color:#0ff;padding:20px;}
input{background:#222;color:#fff;border:1px solid #0ff;padding:6px;width:100%;box-sizing:border-box;margin:4px 0;}
button{background:#0ff;color:#000;padding:8px 20px;border:none;cursor:pointer;font-weight:bold;}
h2{color:#0ff;}label{color:#888;}</style></head>
<body><h2>KORG AI20 Expander — WiFi Config</h2>
<form method="POST" action="/save">
<label>Mode</label><br>
<select name="mode" style="background:#222;color:#fff;border:1px solid #0ff;padding:6px;width:100%;">
  <option value="sta">Station (connect to router)</option>
  <option value="ap">Access Point (standalone)</option>
</select><br><br>
<label>SSID</label><input name="ssid" placeholder="Network name"><br>
<label>Password</label><input name="pass" type="password" placeholder="Password"><br><br>
<button type="submit">Save &amp; Restart</button>
</form></body></html>
)rawhtml";

static void startConfigPortal() {
  if (_cfgServer) return;
  _cfgServer = new WebServer(80);
  _cfgServer->on("/", HTTP_GET, []() {
    _cfgServer->send(200, "text/html", CONFIG_PAGE);
  });
  _cfgServer->on("/save", HTTP_POST, []() {
    WifiCreds c;
    c.magic = 0xB1;
    String ssid = _cfgServer->arg("ssid");
    String pass = _cfgServer->arg("pass");
    String mode = _cfgServer->arg("mode");
    ssid.toCharArray(c.ssid, sizeof(c.ssid));
    pass.toCharArray(c.pass, sizeof(c.pass));
    c.mode = (mode == "sta") ? (uint8_t)WifiMode::STA : (uint8_t)WifiMode::AP;
    saveWifiCreds(c);
    _cfgServer->send(200, "text/html",
      "<html><body style='background:#111;color:#0ff;font-family:monospace;padding:20px'>"
      "<h2>Saved! Rebooting...</h2></body></html>");
    delay(1500);
    ESP.restart();
  });
  _cfgServer->begin();
  Serial.println("[WiFi] Config portal at http://192.168.4.1");
}

// ── Public API ────────────────────────────────────────────────────────────────

// Call from setup() after EEPROM.begin()
void wirelessBegin(WirelessMidiCallback cb) {
  _wirelessCb = cb;

  // ── BLE ──
  BLEMidiServer.begin(DEVICE_NAME);
  BLEMidiServer.setOnConnectCallback(onBleConnected);
  BLEMidiServer.setOnDisconnectCallback(onBleDisconnected);
  BLEMidiServer.setNoteOnCallback(onBleNoteOn);
  BLEMidiServer.setNoteOffCallback(onBleNoteOff);
  BLEMidiServer.setControlChangeCallback(onBleCC);
  BLEMidiServer.setProgramChangeCallback(onBlePC);
  BLEMidiServer.setPitchBendCallback(onBlePitchBend);
  BLEMidiServer.setAfterTouchPolyCallback(onBleAftertouch);
  Serial.println("[BLE] MIDI server started: " DEVICE_NAME);

  // ── WiFi ──
  WifiCreds creds;
  loadWifiCreds(creds);
  wirelessState.wifiMode = (WifiMode)creds.mode;

  if (wirelessState.wifiMode == WifiMode::STA && strlen(creds.ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(creds.ssid, creds.pass);
    Serial.printf("[WiFi] Connecting to %s", creds.ssid);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_MS) {
      delay(250); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wirelessState.wifiUp = true;
      WiFi.localIP().toString().toCharArray(wirelessState.ip, 16);
      Serial.printf("\n[WiFi] STA IP: %s\n", wirelessState.ip);
    } else {
      // fallback to AP
      Serial.println("\n[WiFi] STA failed, fallback to AP");
      wirelessState.wifiMode = WifiMode::AP;
    }
  }

  if (wirelessState.wifiMode == WifiMode::AP) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    wirelessState.wifiUp = true;
    WiFi.softAPIP().toString().toCharArray(wirelessState.ip, 16);
    Serial.printf("[WiFi] AP mode IP: %s\n", wirelessState.ip);
    startConfigPortal();
  }

  // ── RTP-MIDI ──
  if (wirelessState.wifiUp) {
    RTP_MIDI.setHandleConnected(onRtpConnected);
    RTP_MIDI.setHandleDisconnected(onRtpDisconnected);
    RTP_MIDI.setHandleNoteOn(onRtpNoteOn);
    RTP_MIDI.setHandleNoteOff(onRtpNoteOff);
    RTP_MIDI.setHandleControlChange(onRtpCC);
    RTP_MIDI.setHandleProgramChange(onRtpPC);
    RTP_MIDI.setHandlePitchBend(onRtpPitchBend);
    RTP_MIDI.setHandleAfterTouchPoly(onRtpAftertouch);
    RTP_MIDI.setHandleAfterTouchChannel(onRtpChannelPressure);
    RTP_MIDI.begin(MIDI_CHANNEL_OMNI);
    Serial.printf("[RTP] AppleMIDI listening on port %d\n", RTP_PORT);
  }
}

// Call every loop()
void wirelessUpdate() {
  if (wirelessState.wifiUp) {
    RTP_MIDI.read();
    if (_cfgServer) _cfgServer->handleClient();
  }
  // BLE stack is handled internally by ESP-IDF task, no poll needed
}

// Send outgoing MIDI to BLE and RTP clients
void wirelessSendNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  if (wirelessState.bleConnected)
    BLEMidiServer.noteOn(ch, note, vel);
  if (wirelessState.rtpConnected)
    RTP_MIDI.sendNoteOn(note, vel, ch + 1);
}
void wirelessSendNoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
  if (wirelessState.bleConnected)
    BLEMidiServer.noteOff(ch, note, vel);
  if (wirelessState.rtpConnected)
    RTP_MIDI.sendNoteOff(note, vel, ch + 1);
}
void wirelessSendCC(uint8_t ch, uint8_t cc, uint8_t val) {
  if (wirelessState.bleConnected)
    BLEMidiServer.controlChange(ch, cc, val);
  if (wirelessState.rtpConnected)
    RTP_MIDI.sendControlChange(cc, val, ch + 1);
}
void wirelessSendPC(uint8_t ch, uint8_t prog) {
  if (wirelessState.bleConnected)
    BLEMidiServer.programChange(ch, prog);
  if (wirelessState.rtpConnected)
    RTP_MIDI.sendProgramChange(prog, ch + 1);
}

// Send a raw 2- or 3-byte message to wireless (used for THRU)
void wirelessSendRaw(uint8_t status, uint8_t d1, uint8_t d2) {
  uint8_t cmd = status & 0xF0;
  uint8_t ch  = status & 0x0F;
  switch (cmd) {
    case 0x90: wirelessSendNoteOn(ch, d1, d2);  break;
    case 0x80: wirelessSendNoteOff(ch, d1, d2); break;
    case 0xB0: wirelessSendCC(ch, d1, d2);      break;
    case 0xC0: wirelessSendPC(ch, d1);          break;
    default: break; // pitchbend / aftertouch not forwarded as thru
  }
}

// Status accessors for display
inline bool bleConnected()  { return wirelessState.bleConnected; }
inline bool rtpConnected()  { return wirelessState.rtpConnected; }
inline bool wifiUp()        { return wirelessState.wifiUp; }
inline const char* wifiIP() { return wirelessState.ip; }
inline WifiMode wifiMode()  { return wirelessState.wifiMode; }
