# Korg AI20 Wavetable MIDI Expander

ESP32-basierter MIDI Expander für das Korg AI20 (AI202) Wavetable-Daughterboard.

## Features

- **MIDI DIN IN** → AI20 (mit PC817 Optokoppler, galvanisch getrennt)
- **MIDI DIN OUT/THRU** (mit PC817, galvanisch getrennt)
- **USB MIDI** (Akai MPK mini) → AI20 (via Mini USB Host Shield 2.0)
- **ST7789 240x240 Display** mit UI
- **Rotary Encoder** für Kanal, Programm, Lautstärke, Transpose
- **MIDI Routing:** Beide MIDI-Quellen (DIN + USB) werden zusammengeführt und an den AI20 geroutet
- **Transpose** semitonal (±64 Halbtöne)

## Hardware-Liste

| Bauteil                          | Menge |
|----------------------------------|-------|
| ESP32 DevKit (WROOM-32)         | 1     |
| Korg AI20 Daughterboard          | 1     |
| 26-Pin Waveblaster Header (Buchse) | 1   |
| ST7789 240x240 SPI Display       | 1     |
| Mini USB Host Shield 2.0         | 1     |
| PC817 Optokoppler                | 2     |
| Rotary Encoder (KY-040)          | 1     |
| 1N4148 Diode                     | 1     |
| Widerstand 220Ω                  | 3     |
| Widerstand 470Ω                  | 1     |
| Widerstand 4.7kΩ                 | 1     |
| Widerstand 10kΩ                  | 4     |
| Kondensator 100nF                | 1     |
| 5-Pin DIN-Buchse                 | 2     |
| BSS138 oder 74HCT04 (Pegelwandler) | 1   |

## Software / Dependencies

PlatformIO mit espressif32 Platform.

Libraries (in `platformio.ini`):
- `Adafruit ST7789`
- `Adafruit GFX Library`
- `USB Host Shield Library 2.0` (felis fork, ESP32-kompatibel)

## Build

```bash
pio run --target upload
pio device monitor
```

## Verzeichnisstruktur

```
korg_ai20_expander/
├── platformio.ini
├── src/
│   └── main.cpp
└── docs/
    └── SCHEMATIC.md    ← Vollständiger Schaltplan + Verdrahtung
```

## Funktionsweise

1. **Boot:** AI20 wird mit GM System On SysEx initialisiert
2. **Encoder:** Drücken wechselt zwischen Navigation und Edit-Modus
3. **DIN MIDI IN:** UART2 RX empfängt MIDI, parsed und routet an AI20 (UART1 TX)
4. **USB MIDI IN:** USB Host Shield empfängt USB-MIDI Pakete vom Akai MPK mini
5. **Transpose:** Wird auf alle Note On/Off Messages angewendet bevor sie an AI20 gehen
6. **Display:** Zeigt Kanal, Programm, Lautstärke, Transpose und MIDI-Aktivitäts-LEDs

## Bekannte Einschränkungen

- SysEx-Durchleitung ist nicht implementiert (nur Standard MIDI Messages)
- Gleichzeitige Note-Priority zwischen DIN und USB: Last-wins (kein Voice-Stealing)
- USB Host Shield SPI-Konflikt mit zweitem SPI-Bus möglich wenn selber HSPI genutzt wird → deshalb VSPI für Display, HSPI für USB Shield
