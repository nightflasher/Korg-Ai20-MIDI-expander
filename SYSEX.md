# AI202 / AG-10 Extended MIDI & SysEx Reference

## Was der Chip tatsächlich kann (über GM hinaus)

### Soundbänke

| Bank        | CC0 | CC32 | Programme    | Beschreibung                            |
|-------------|-----|------|--------------|-----------------------------------------|
| GM          |  0  |  0   | 0–127        | Standard General MIDI                   |
| VAR         |  0  |  1   | je nach Prog | GS-style Variationen (nicht alle PCs belegt) |
| USR/Custom  | 62  |  0   | 96 (0-based) | Ein SysEx-programmierbares Custom-Patch |
| Drum GM     |  0  |  0   | Ch10 PC 0    | Standard GM Drum Kit                    |
| Drum Power  |  0  |  0   | Ch10 PC 8    | Power Kit                               |
| Drum Analog |  0  |  0   | Ch10 PC 16   | Analog Kit                              |
| Drum Brush  |  0  |  0   | Ch10 PC 24   | Brush Kit                               |

### Extended CCs (über GM hinaus, laut AG-10 MIDI Spec)

| CC  | Funktion              | Wertebereich |
|-----|-----------------------|--------------|
|  1  | Modulation            | 0–127        |
|  7  | Volume                | 0–127        |
| 10  | Pan                   | 0–127 (64=Mitte) |
| 11  | Expression            | 0–127        |
| 64  | Sustain Pedal         | 0/127        |
| 91  | Reverb Send Level     | 0–127        |
| 93  | Chorus Send Level     | 0–127        |
| 94  | Variation/Celeste     | 0–127        |
| 120 | All Sound Off         | 0            |
| 121 | Reset All Controllers | 0            |
| 123 | All Notes Off         | 0            |

### NRPN (Non-Registered Parameter Numbers) – AI202

Die AI202 unterstützt NRPNs für Feinsteuerung per Kanal.
Sequenz: CC 99 (MSB) → CC 98 (LSB) → CC 6 (Data Entry MSB)

| NRPN MSB | NRPN LSB | Funktion                  | Bereich |
|----------|----------|---------------------------|---------|
|    1     |   8      | Vibrato Rate              | 0–127   |
|    1     |   9      | Vibrato Depth             | 0–127   |
|    1     |  10      | Vibrato Delay             | 0–127   |
|    1     |  32      | Filter Cutoff (Brightness)| 0–127   |
|    1     |  33      | Filter Resonance          | 0–127   |
|    1     |  99      | Attack Time               | 0–127   |
|    1     |  100     | Decay Time                | 0–127   |
|    1     |  102     | Release Time              | 0–127   |
|  127     |  127     | NRPN Null (cancel)        | –       |

Beispiel – Vibrato erhöhen auf Kanal 1:
```
B0 63 01  (CC99 MSB = 1)
B0 62 08  (CC98 LSB = 8 → Vibrato Rate)
B0 06 60  (CC6 Data = 96)
B0 63 7F  (NRPN Null)
B0 62 7F
```

---

## Custom Patch (Bank 62, PC 96) via SysEx

Der AI202 hat einen programmierbaren Patch-Slot (selektiert via CC0=62, CC32=0, PC=96).
Das Patch wird über Korg-proprietäres SysEx mit allen AI2-Synthese-Parametern beschrieben.

### SysEx Format (Korg AI2 / AG-10)

```
F0  42  3n  00  01  [parameter_id]  [value]  F7

F0       = SysEx Start
42       = Korg Manufacturer ID
3n       = 30 + MIDI Channel (0-based), z.B. 30 für CH1
00 01    = AG-10 Device/Model ID
[param]  = Parameter-Nummer (1 Byte)
[value]  = Wert (1 Byte, 7-bit)
F7       = SysEx End
```

### Bekannte AI2-Patch-Parameter IDs

| ID (hex) | Parameter               | Bereich | Default |
|----------|-------------------------|---------|---------|
| 00       | Oscillator 1 Waveform   | 0–127   | –       |
| 01       | Oscillator 2 Waveform   | 0–127   | –       |
| 02       | Osc Mix                 | 0–127   | 64      |
| 03       | Osc Pitch (Coarse)      | 0–127   | 64      |
| 04       | Osc Pitch Fine          | 0–127   | 64      |
| 10       | Filter Cutoff           | 0–127   | 100     |
| 11       | Filter Resonance        | 0–127   | 0       |
| 12       | Filter EG Depth         | 0–127   | 64      |
| 20       | Amp EG Attack           | 0–127   | 0       |
| 21       | Amp EG Decay            | 0–127   | 64      |
| 22       | Amp EG Sustain          | 0–127   | 100     |
| 23       | Amp EG Release          | 0–127   | 40      |
| 30       | Reverb Type             | 0–7     | 2       |
| 31       | Reverb Depth            | 0–127   | 40      |
| 32       | Chorus Type             | 0–7     | 0       |
| 33       | Chorus Depth            | 0–127   | 0       |
| 40       | Velocity Sensitivity    | 0–127   | 64      |
| 41       | Pitch EG Depth          | 0–127   | 64      |

⚠️ **Hinweis:** Die exakten Parameter-IDs für den AI202 sind nicht vollständig
öffentlich dokumentiert. Die obige Tabelle basiert auf Community-Reverse-Engineering
(VOGONS-Forum / CHiLL-Projekt). Die `.AG`-Dateien aus der AG-10 Sound Editor
Software (X-Technology TopWave 32 Paket) sind die zuverlässigste Referenz.

### Custom Patch selektieren

```
// Bank select: CC0=62, CC32=0 auf gewünschtem Kanal
B0 00 3E   (CC0 = 62)
B0 20 00   (CC32 = 0)
C0 60      (PC = 96, 0-based)
```

---

## Reverb- und Chorus-Typen (AG-10 / AI202)

### Reverb Typen (CC91-Pegel steuert Send-Level)
Typ wird per SysEx Parameter 0x30 gesetzt:

| Wert | Typ           |
|------|---------------|
|  0   | Room 1        |
|  1   | Room 2        |
|  2   | Hall 1        |
|  3   | Hall 2        |
|  4   | Plate         |
|  5   | Delay         |
|  6   | Panning Delay |

### Chorus Typen (CC93-Pegel steuert Send-Level)
Typ wird per SysEx Parameter 0x32 gesetzt:

| Wert | Typ      |
|------|----------|
|  0   | Chorus 1 |
|  1   | Chorus 2 |
|  2   | Chorus 3 |
|  3   | Chorus 4 |
|  4   | Feedback |
|  5   | Flanger  |

---

## GM System On / System Off

```
// GM System On (Reset + GM-Modus aktivieren)
F0 7E 7F 09 01 F7

// GM System Off
F0 7E 7F 09 02 F7
```

Nach GM System On: Alle Kanäle auf Default zurückgesetzt,
CH10 = Drums, alle anderen = Instrument.
Wartezeit danach: mind. 50ms.

---

## Referenzen

- VOGONS Forum: "Korg Midi Daughterboards" Thread (besonders Page 3)
- Internet Archive: "Korg Audio Gallery AG-10 Resources" (AG to SYX converter + .AG files)
- CHiLL Solder Kit Doku: serdashop.com
- Creative Wave Blaster Wikipedia (Waveblaster Pinout)
- joska.atari.org: Waveblaster Pinout + MIDI TTL schematic
