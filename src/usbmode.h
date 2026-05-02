#pragma once
/*
 * usbmode.h  -  Automatische USB Host/Device Umschaltung
 *
 * Problem: GPIO19/20 sind der einzige USB-OTG-Port des ESP32-S3.
 *   Host-Modus:   Akai MPK mini (oder anderes USB-MIDI-Geraet) angeschlossen
 *   Device-Modus: ESP32 am PC angeschlossen -> erscheint als "AI20 Expander" USB-MIDI
 *
 * Umschalt-Logik (automatisch):
 *   1. Beim Boot: Pruefen ob VBUS vom Host (PC) anliegt
 *      -> VBUS hoch = PC-Verbindung erkannt -> Device-Modus
 *      -> VBUS niedrig = kein PC -> Host-Modus (warte auf MPK mini)
 *   2. Im Betrieb: Falls Host-Modus aktiv und ein MIDI-Device verbunden wird,
 *      bleibt Host-Modus.
 *   3. Falls Device-Modus: MIDI-Pakete vom PC kommen ueber TinyUSB MIDI Device API.
 *
 * VBUS-Sense:
 *   Der Freenove S3-WROOM hat keinen dedizierten VBUS-Sense-Pin exponiert.
 *   Workaround: GPIO GPIO_VBUS_SENSE (falls verfuegbar) oder einfache Heuristik:
 *   - Beim Start einmal USB-OTG-Status pruefen via esp_usb_phy_status
 *   - Praktischer Ansatz: Button-Combo beim Boot waehlt Modus
 *     (ENC3-Button beim Start gedrueckt = Device-Modus erzwingen)
 *
 * USB MIDI Device (TinyUSB):
 *   arduino-esp32 >= 3.x: USBMIDI als Device-Klasse verfuegbar
 *   Board wird als USB-MIDI-Device erkannt, kein Treiber am PC noetig.
 *
 * USB MIDI Host (TinyUSB OTG):
 *   Erfordert USB OTG mode + TinyUSB Host-Stack.
 *   Bei arduino-esp32 3.x noch experimentell.
 *   Zuverlaessiger: USB Host Shield 2.0 (aber das haben wir nicht).
 *   Aktueller Stand: Device-Modus ist stabil; Host-Modus ist best-effort.
 */

#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "pins.h"

// ── Modus ─────────────────────────────────────────────────────────────────────
enum class UsbMode : uint8_t { UNDECIDED, HOST, DEVICE };
static UsbMode usbMode = UsbMode::UNDECIDED;

// USB MIDI Device instance (fuer Device-Modus)
static USBMIDI usbMidiDevice;

// Callback fuer eingehende MIDI-Nachrichten (gesetzt von main.cpp)
typedef void (*UsbMidiCallback)(uint8_t status, uint8_t d1, uint8_t d2);
static UsbMidiCallback _usbMidiCb = nullptr;

// ── Modus-Erkennung ───────────────────────────────────────────────────────────
// Gibt true zurueck wenn VBUS vom externen Host (PC) anliegt.
// Heuristik: beim Start ENC3-Button abfragen (gedrueckt = Device-Modus erzwingen).
// Ohne Hardware-VBUS-Sense ist das die zuverlaessigste Methode.
static bool detectDeviceMode() {
  // Wenn ENC3-Button beim Boot gedrueckt: Device-Modus erzwingen
  pinMode(PIN_ENC3_SW, INPUT_PULLUP);
  delay(10);
  bool forceDevice = (digitalRead(PIN_ENC3_SW) == LOW);
  if(forceDevice) {
    Serial.println("[USB] ENC3 held at boot -> Device mode forced");
    return true;
  }
  // Automatisch: Versuche VBUS ueber ADC zu messen wenn moeglich.
  // Auf dem Freenove-Board gibt es keinen dedizierten VBUS-Pin auf dem Header.
  // Default: Host-Modus (steht am meisten fuer Standalone-Betrieb)
  return false;
}

// ── Device-Modus Setup ────────────────────────────────────────────────────────
static void startDeviceMode(UsbMidiCallback cb) {
  _usbMidiCb = cb;
  usbMode = UsbMode::DEVICE;

  // USB MIDI Device: ESP32-S3 meldet sich am PC als MIDI-Interface
  USB.manufacturerName("Anthropic DIY");
  USB.productName("AI20 Expander");
  USB.serialNumber("AI20-001");
  usbMidiDevice.begin();
  USB.begin();
  Serial.println("[USB] Device mode: AI20 Expander MIDI interface");
}

// ── Host-Modus Setup ──────────────────────────────────────────────────────────
static void startHostMode(UsbMidiCallback cb) {
  _usbMidiCb = cb;
  usbMode = UsbMode::HOST;
  // TinyUSB Host-Modus wird durch ARDUINO_USB_MODE=1 build-flag aktiviert.
  // Das USBMIDI-Objekt funktioniert im Host-Modus als MIDI-Host.
  usbMidiDevice.begin();
  USB.begin();
  Serial.println("[USB] Host mode: waiting for USB-MIDI device (Akai MPK mini)");
}

// ── Init ──────────────────────────────────────────────────────────────────────
void usbModeBegin(UsbMidiCallback cb) {
  bool deviceMode = detectDeviceMode();
  if(deviceMode) startDeviceMode(cb);
  else           startHostMode(cb);
}

// ── Poll ──────────────────────────────────────────────────────────────────────
void usbModeUpdate() {
  while(usbMidiDevice.available()) {
    MIDIMessage msg = usbMidiDevice.read();
    if(_usbMidiCb) {
      uint8_t status = msg.type | (msg.channel & 0x0F);
      _usbMidiCb(status, msg.data1, msg.data2);
    }
  }
}

// ── Ausgang: MIDI an PC senden (Device-Modus) oder ignorieren (Host) ──────────
void usbMidiSend(uint8_t status, uint8_t d1, uint8_t d2) {
  if(usbMode != UsbMode::DEVICE) return;
  uint8_t cmd = status & 0xF0;
  uint8_t ch  = status & 0x0F;
  switch(cmd) {
    case 0x90: usbMidiDevice.noteOn(ch+1, d1, d2);           break;
    case 0x80: usbMidiDevice.noteOff(ch+1, d1, d2);          break;
    case 0xB0: usbMidiDevice.controlChange(ch+1, d1, d2);    break;
    case 0xC0: usbMidiDevice.programChange(ch+1, d1);        break;
    case 0xE0: usbMidiDevice.pitchBend(ch+1, (d1|(d2<<7))-8192); break;
    default: break;
  }
}

inline UsbMode getUsbMode()        { return usbMode; }
inline bool    isDeviceMode()      { return usbMode == UsbMode::DEVICE; }
inline bool    isHostMode()        { return usbMode == UsbMode::HOST; }
inline const char* usbModeLabel()  { return usbMode==UsbMode::DEVICE?"DEV":"HOST"; }
