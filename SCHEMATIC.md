# Korg AI20 MIDI Expander – Hardware Dokumentation

## Übersicht

Der Korg AI20 ist eine Waveblaster-kompatible Daughterboard mit 26-pin Header.
Das Board enthält den KORG AI202-Chip (32-Note polyphon, 128 GM-Sounds).
Das ESP32 übernimmt:
- MIDI-Routing von DIN IN und USB (Akai MPK mini) → AI20
- MIDI THRU/OUT über DIN
- Display-UI mit Encoder

---

## Waveblaster 26-Pin Header Pinout

```
Pin  Signal        Beschreibung
---  ------        -------------
1    NC            nicht belegt
2    GND
3    MIDI IN       TTL-Pegel (nicht 5V-Strombegrenzt wie DIN!)
4    GND
5    +5V
6    GND
7    NC
8    GND
9    +5V
10   GND
11   NC
12   GND
13   +5V
14   NC
15   NC
16   GND
17   NC
18   GND
19   AUDIO RIGHT   Line Level (~1Vpp)
20   GND
21   -12V          (nur ältere Boards – AI20 braucht das NICHT)
22   GND
23   AUDIO LEFT    Line Level (~1Vpp)
24   GND
25   RESET         Active LOW (10kΩ nach +5V → immer HIGH = läuft)
26   GND
```

**Wichtig:** Pin 3 ist TTL (0V/5V), kein MIDI-Current-Loop.
Der ESP32 GPIO 17 (3.3V) muss über einen **Pegelwandler** auf 5V angehoben werden,
oder direkt angeschlossen werden – der AI202-Chip ist laut Community 3.3V-tolerant,
funktioniert aber sicherer mit einem 74HCT04 Inverter-Paar oder einem BSS138 N-FET Shifter.

---

## Stromversorgung des AI20

Der Korg AI20 benötigt NUR +5V (Pins 5, 9, 13).
-12V und +12V werden vom Board NICHT benötigt.
Strom: ca. 150–200mA typisch.

Empfehlung: ESP32 Dev Board über USB versorgen (5V), AI20 vom selben 5V Rail
über einen ausreichend dimensionierten LDO oder direkt vom USB-5V-Rail.

---

## MIDI DIN IN/OUT via PC817

### MIDI IN (DIN → ESP32)

```
DIN Pin 4 (+) ──┐
                ├── +5V über 220Ω
DIN Pin 5 (-) ──┤── 1N4148 (Schutzdiode, Anode zu Pin5)
                └── LED-Seite PC817 (Anode = Pin4, Kathode = Pin5 nach Widerstand)

PC817 Emitter → GND
PC817 Kollektor → 3.3V über 4.7kΩ → ESP32 GPIO 16 (UART2 RX)
```

Detailschaltung:
```
             220Ω     1N4148
DIN-4 ──────/\/\/──┬──|>|──┐
                   │        │
                  [PC817]   │
                   │        │
DIN-5 ─────────────┘        │
                            └── GND (Kathode)

PC817-Kollektor ──── 4.7kΩ ──── 3.3V
PC817-Kollektor ──────────────── GPIO 16
PC817-Emitter   ──────────────── GND
```

### MIDI OUT (ESP32 → DIN)

```
ESP32 GPIO 4 (UART2 TX) → 220Ω → Basis NPN (z.B. 2N3904 oder BC547)
NPN Emitter → GND
NPN Kollektor → 220Ω → DIN Pin 5 (Ausgang)
                      ↑ auch 220Ω nach +5V → DIN Pin 4

Alternativ direkt PC817:
ESP32 GPIO 4 → 470Ω → PC817 LED+ (Anode)
                       PC817 LED- (Kathode) → GND
PC817 Kollektor → 220Ω → DIN Pin 5
PC817 Emitter   → GND
+5V → 220Ω → DIN Pin 4
```

Standard MIDI OUT mit PC817:
```
+5V ──── 220Ω ──── DIN-4

GPIO4 ─── 470Ω ─── [PC817-LED+]
                    [PC817-LED-] ─── GND
PC817-Kollektor ─── 220Ω ─── DIN-5
PC817-Emitter   ─── GND
```

---

## USB Host Shield 2.0 (Mini) – ESP32 SPI Verdrahtung

Der USB Host Shield 2.0 läuft normalerweise auf 5V Logik (AVR).
Für ESP32 (3.3V) gibt es zwei Wege:
1. **Mini USB Host Shield 2.0 von Circuits@Home** – hat 3.3V-Betrieb-Option (JP1 Jumper)
2. Pegelwandler auf allen SPI-Leitungen

Empfehlung: Board mit JP1-Jumper auf 3.3V stellen (VBUS bleibt 5V für USB-Gerät).

```
USB Shield       ESP32 (HSPI)
---------        ------------
SCK   ────────── GPIO 14
MISO  ────────── GPIO 12
MOSI  ────────── GPIO 13
SS    ────────── GPIO 26
INT   ────────── GPIO 25
3.3V  ────────── 3.3V
GND   ────────── GND
```

Der USB-Port des Shields (Type-A) wird direkt mit dem Akai MPK mini verbunden.
Das Shield übernimmt die USB-Enumeration und MIDI-Class-Treiber automatisch.

---

## ST7789 Display (240x240) SPI

Läuft auf 3.3V, direkt an ESP32.

```
ST7789           ESP32 (VSPI)
-------          ------------
SCL/SCK ──────── GPIO 18
SDA/MOSI ─────── GPIO 23
CS  ──────────── GPIO 5
DC  ──────────── GPIO 2
RES/RST ──────── GPIO 15
BLK/BL  ──────── GPIO 27 (PWM, über 100Ω Widerstand)
VCC ──────────── 3.3V
GND ──────────── GND
```

---

## Rotary Encoder (KY-040 oder äquivalent)

```
Encoder          ESP32
-------          -----
CLK ──────────── GPIO 34 (Input only, intern Pull-up nicht möglich → 10kΩ extern nach 3.3V)
DT  ──────────── GPIO 35 (Input only, intern Pull-up nicht möglich → 10kΩ extern nach 3.3V)
SW  ──────────── GPIO 32 (10kΩ nach 3.3V, 100nF nach GND für Entprellung)
+   ──────────── 3.3V
GND ──────────── GND
```

Hinweis: GPIO 34 und 35 haben keinen internen Pull-up. Externe 10kΩ sind Pflicht.

---

## Pegelwandler ESP32 (3.3V) → AI20 Pin 3 (5V TTL)

Option A – BSS138 N-FET Bidirektional (SparkFun Logic Level Converter):
```
3.3V ──── 10kΩ ──── FET-Gate&Source (LV-Seite)
GPIO 17 ──────────── LV-Pin
5V   ──── 10kΩ ──── FET-Drain (HV-Seite)
HV-Pin ─────────────── Waveblaster Pin 3
```

Option B – 74HCT04 (einfacher):
```
GPIO 17 → 74HCT04 Eingang (HCT akzeptiert 3.3V als HIGH)
74HCT04 Ausgang A → 74HCT04 Eingang B (doppelter Inverter = Puffer, 5V Ausgang)
74HCT04 Ausgang B → Waveblaster Pin 3
```

Option C – Direkt (Risiko):
GPIO 17 direkt an Waveblaster Pin 3.
Community-Berichte zeigen dass AI202 meist 3.3V-tolerant ist, aber nicht garantiert.

---

## Gesamte Stromverteilung

```
USB 5V (extern)
   │
   ├── ESP32 VIN (5V)
   ├── USB Host Shield VBUS (5V, für Akai)
   ├── AI20 Waveblaster Pins 5/9/13 (+5V)
   └── MIDI OUT Pull-up (+5V)

ESP32 3.3V (intern LDO)
   ├── ST7789 VCC
   ├── Encoder VCC
   └── USB Shield (wenn JP1 auf 3.3V)
```

Gesamtstrom: ~500mA peak (AI20 ~200mA + ESP32 ~100mA + USB Shield ~50mA + Akai MPK mini ~100mA)
→ USB 2.0 (500mA) ist grenzwertig, besser ein Netzteil mit 1A verwenden.

---

## Wichtige Hinweise / Upgrade-Empfehlungen

### ⚠ USB Host Shield Kompatibilität
Das **Mini USB Host Shield 2.0** von Circuits@Home funktioniert mit dem ESP32,
ABER: Es gibt Berichte über Timing-Probleme bei hoher SPI-Frequenz.
SPI-Takt auf max. 8 MHz begrenzen (in der USB_Host_Shield_2.0 Library: `SPI_INIT_SPEED`).

### ⚠ Netzteil
Wenn du alles über den ESP32 USB-Anschluss versorgen willst:
Ein normales Handy-Ladekabel liefert 500mA–1A. Bei 500mA kann es knapp werden.
Empfehlung: **5V/1.5A Netzteil mit Barrel-Jack** und Verteiler.

### ℹ Audio-Ausgang des AI20
Pins 19/23 liefern Line-Level (~1Vpp).
Für Kopfhörer: kleiner LM386 oder TPA2012 Verstärker-IC nachschalten.
Für Mischer/Interface: direkt nutzbar.

---

## Nicht nötige Upgrades

- **Level-Shifter für SPI (Display/USB):** ESP32 VSPI/HSPI ist 3.3V, 
  ST7789 und das Shield (bei 3.3V-Betrieb) laufen direkt.
- **Externer Quarz:** ESP32 interner Oszillator reicht für MIDI (31250 Baud).
- **DAC:** AI20 hat eigenen internen DAC.
