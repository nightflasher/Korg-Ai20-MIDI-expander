#pragma once
/*
 * pins.h  -  Freenove ESP32-S3-WROOM N16R8 CAM
 *
 * Kamera-Pins (nach Entfernen der Kamera frei):
 *   XCLK=0, SDA=21, SCL=22, PCLK=33, VSYNC=34, HREF=35(intern!),
 *   D0-D6=36-42(intern!), D7=43(UART0-TX!)
 *   -> Von den Kamera-Pins sind nach Entfernen nutzbar:
 *      GPIO0(Strapping!), GPIO21, GPIO22, GPIO33, GPIO34
 *      GPIO36-42 sind INTERN fuer Octal-PSRAM -> NICHT verwenden
 *      GPIO43/44 = UART0 TX/RX -> fuer Serial-Konsole behalten
 *
 * SD-Karte (onboard SDMMC, nicht aenderbar):
 *   CLK=38, CMD=39, D0=40
 *
 * USB OTG (fest):
 *   D-=19, D+=20
 *
 * Gesperrte Pins (NIE verwenden):
 *   GPIO35, GPIO36, GPIO37  -> Octal PSRAM intern
 *   GPIO43, GPIO44          -> UART0 (Serial-Konsole / Flash)
 *   GPIO0, GPIO3, GPIO45, GPIO46 -> Strapping (vorsichtig)
 */

// ── ILI9341 Display (SPI2/FSPI, IOMUX-Pfad) ──────────────────────────────────
#define PIN_TFT_SCK    12
#define PIN_TFT_MOSI   11
#define PIN_TFT_CS     10
#define PIN_TFT_DC      8
#define PIN_TFT_RST     9
#define PIN_TFT_BL     13   // LEDC PWM Backlight

// ── Encoder 1: Navigation (Menü-Cursor) ──────────────────────────────────────
#define PIN_ENC1_CLK   14   // A  - externes 10k nach 3.3V!
#define PIN_ENC1_DT    15   // B  - externes 10k nach 3.3V!
#define PIN_ENC1_SW    16   // SW - interner Pullup reicht

// ── Encoder 2: Programm / Bank ────────────────────────────────────────────────
#define PIN_ENC2_CLK   17   // A  - externes 10k nach 3.3V!
#define PIN_ENC2_DT    18   // B  - externes 10k nach 3.3V!
#define PIN_ENC2_SW     7   // SW

// ── Encoder 3: Volume ─────────────────────────────────────────────────────────
#define PIN_ENC3_CLK    1   // A  - externes 10k nach 3.3V!
#define PIN_ENC3_DT     2   // B  - externes 10k nach 3.3V!
#define PIN_ENC3_SW     4   // SW

// ── UART1: AI20 Waveblaster MIDI TTL ─────────────────────────────────────────
// ACHTUNG: 3.3V->5V Pegelwandler (BSS138 oder 74HCT04) noetig!
#define PIN_MIDI_AI20_TX   21   // -> Waveblaster Pin 3

// ── UART2: DIN MIDI IN/OUT via PC817 ─────────────────────────────────────────
#define PIN_MIDI_DIN_RX    22   // <- PC817 MIDI IN
#define PIN_MIDI_DIN_TX    23   // -> PC817 MIDI OUT

// ── SD-Karte (SDMMC onboard, Pins fest auf Board) ────────────────────────────
#define PIN_SD_CLK     38
#define PIN_SD_CMD     39
#define PIN_SD_D0      40

// ── USB OTG (fest, nicht aenderbar) ──────────────────────────────────────────
#define PIN_USB_DM     19   // D-
#define PIN_USB_DP     20   // D+

// ── Frei fuer spaetere Erweiterung ───────────────────────────────────────────
// GPIO5, GPIO6, GPIO24, GPIO25, GPIO26, GPIO27, GPIO28, GPIO29,
// GPIO30, GPIO31, GPIO32, GPIO33, GPIO34, GPIO41, GPIO42, GPIO47, GPIO48
