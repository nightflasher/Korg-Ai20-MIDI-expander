#pragma once
#include <pgmspace.h>

// ── AI20 / AG-10 Sound Database ────────────────────────────────────────────────
//
// The AI202 chip is identical to the AG-10 engine:
//   • Bank GM  (CC0=0,  CC32=0): 128 General MIDI programs + 4 Drum Kits
//   • Bank VAR (CC0=0,  CC32=1): Korg variation sounds (mostly GS-style variants)
//   • Bank USR (CC0=62, CC32=0): Custom patch slot at PC=96 (prog 97 in 1-based)
//                                Programmed via SysEx (all AI202 synth params)
//
// Bank select protocol (Korg mode):
//   CC0  (MSB) → value
//   CC32 (LSB) → value
//   PC         → program (0-based)
//
// Drum kits are always on MIDI channel 10 (index 9).
// Drum program changes: PC 0 = GM Kit, 8 = Power Kit, 16 = Analog Kit, 24 = Brush Kit
// ────────────────────────────────────────────────────────────────────────────────

struct SoundEntry {
  uint8_t  cc0;
  uint8_t  cc32;
  uint8_t  pc;      // 0-based
  const char* name;
};

// ── GM Bank (CC0=0, CC32=0) ─ 128 programs ────────────────────────────────────
static const SoundEntry GM_SOUNDS[] PROGMEM = {
  // Piano (0-7)
  {0, 0,   0, "Acoustic Grand"},
  {0, 0,   1, "Bright Acoustic"},
  {0, 0,   2, "Electric Grand"},
  {0, 0,   3, "Honky-Tonk"},
  {0, 0,   4, "Electric Piano 1"},
  {0, 0,   5, "Electric Piano 2"},
  {0, 0,   6, "Harpsichord"},
  {0, 0,   7, "Clavi"},
  // Chromatic Perc (8-15)
  {0, 0,   8, "Celesta"},
  {0, 0,   9, "Glockenspiel"},
  {0, 0,  10, "Music Box"},
  {0, 0,  11, "Vibraphone"},
  {0, 0,  12, "Marimba"},
  {0, 0,  13, "Xylophone"},
  {0, 0,  14, "Tubular Bells"},
  {0, 0,  15, "Dulcimer"},
  // Organ (16-23)
  {0, 0,  16, "Drawbar Organ"},
  {0, 0,  17, "Percussive Organ"},
  {0, 0,  18, "Rock Organ"},
  {0, 0,  19, "Church Organ"},
  {0, 0,  20, "Reed Organ"},
  {0, 0,  21, "Accordion"},
  {0, 0,  22, "Harmonica"},
  {0, 0,  23, "Bandoneon"},
  // Guitar (24-31)
  {0, 0,  24, "Nylon Guitar"},
  {0, 0,  25, "Steel Guitar"},
  {0, 0,  26, "Jazz Guitar"},
  {0, 0,  27, "Clean Guitar"},
  {0, 0,  28, "Muted Guitar"},
  {0, 0,  29, "Overdriven Gtr"},
  {0, 0,  30, "Distortion Gtr"},
  {0, 0,  31, "Guitar Harmonic"},
  // Bass (32-39)
  {0, 0,  32, "Acoustic Bass"},
  {0, 0,  33, "Finger Bass"},
  {0, 0,  34, "Pick Bass"},
  {0, 0,  35, "Fretless Bass"},
  {0, 0,  36, "Slap Bass 1"},
  {0, 0,  37, "Slap Bass 2"},
  {0, 0,  38, "Synth Bass 1"},
  {0, 0,  39, "Synth Bass 2"},
  // Strings (40-47)
  {0, 0,  40, "Violin"},
  {0, 0,  41, "Viola"},
  {0, 0,  42, "Cello"},
  {0, 0,  43, "Contrabass"},
  {0, 0,  44, "Tremolo Strings"},
  {0, 0,  45, "Pizzicato Strngs"},
  {0, 0,  46, "Orchestral Harp"},
  {0, 0,  47, "Timpani"},
  // Ensemble (48-55)
  {0, 0,  48, "String Ensemble1"},
  {0, 0,  49, "String Ensemble2"},
  {0, 0,  50, "Synth Strings 1"},
  {0, 0,  51, "Synth Strings 2"},
  {0, 0,  52, "Choir Aahs"},
  {0, 0,  53, "Voice Oohs"},
  {0, 0,  54, "Synth Voice"},
  {0, 0,  55, "Orchestra Hit"},
  // Brass (56-63)
  {0, 0,  56, "Trumpet"},
  {0, 0,  57, "Trombone"},
  {0, 0,  58, "Tuba"},
  {0, 0,  59, "Muted Trumpet"},
  {0, 0,  60, "French Horn"},
  {0, 0,  61, "Brass Section"},
  {0, 0,  62, "Synth Brass 1"},
  {0, 0,  63, "Synth Brass 2"},
  // Reed (64-71)
  {0, 0,  64, "Soprano Sax"},
  {0, 0,  65, "Alto Sax"},
  {0, 0,  66, "Tenor Sax"},
  {0, 0,  67, "Baritone Sax"},
  {0, 0,  68, "Oboe"},
  {0, 0,  69, "English Horn"},
  {0, 0,  70, "Bassoon"},
  {0, 0,  71, "Clarinet"},
  // Pipe (72-79)
  {0, 0,  72, "Piccolo"},
  {0, 0,  73, "Flute"},
  {0, 0,  74, "Recorder"},
  {0, 0,  75, "Pan Flute"},
  {0, 0,  76, "Blown Bottle"},
  {0, 0,  77, "Shakuhachi"},
  {0, 0,  78, "Whistle"},
  {0, 0,  79, "Ocarina"},
  // Synth Lead (80-87)
  {0, 0,  80, "Square Lead"},
  {0, 0,  81, "Sawtooth Lead"},
  {0, 0,  82, "Calliope Lead"},
  {0, 0,  83, "Chiff Lead"},
  {0, 0,  84, "Charang Lead"},
  {0, 0,  85, "Voice Lead"},
  {0, 0,  86, "Fifths Lead"},
  {0, 0,  87, "Bass+Lead"},
  // Synth Pad (88-95)
  {0, 0,  88, "New Age Pad"},
  {0, 0,  89, "Warm Pad"},
  {0, 0,  90, "Polysynth Pad"},
  {0, 0,  91, "Choir Pad"},
  {0, 0,  92, "Bowed Pad"},
  {0, 0,  93, "Metallic Pad"},
  {0, 0,  94, "Halo Pad"},
  {0, 0,  95, "Sweep Pad"},
  // Synth Effects (96-103)
  {0, 0,  96, "Rain FX"},
  {0, 0,  97, "Soundtrack FX"},
  {0, 0,  98, "Crystal FX"},
  {0, 0,  99, "Atmosphere FX"},
  {0, 0, 100, "Brightness FX"},
  {0, 0, 101, "Goblins FX"},
  {0, 0, 102, "Echoes FX"},
  {0, 0, 103, "Sci-Fi FX"},
  // Ethnic (104-111)
  {0, 0, 104, "Sitar"},
  {0, 0, 105, "Banjo"},
  {0, 0, 106, "Shamisen"},
  {0, 0, 107, "Koto"},
  {0, 0, 108, "Kalimba"},
  {0, 0, 109, "Bag Pipe"},
  {0, 0, 110, "Fiddle"},
  {0, 0, 111, "Shanai"},
  // Percussive (112-119)
  {0, 0, 112, "Tinkle Bell"},
  {0, 0, 113, "Agogo"},
  {0, 0, 114, "Steel Drums"},
  {0, 0, 115, "Woodblock"},
  {0, 0, 116, "Taiko Drum"},
  {0, 0, 117, "Melodic Tom"},
  {0, 0, 118, "Synth Drum"},
  {0, 0, 119, "Reverse Cymbal"},
  // Sound Effects (120-127)
  {0, 0, 120, "Guitar Fret Nois"},
  {0, 0, 121, "Breath Noise"},
  {0, 0, 122, "Seashore"},
  {0, 0, 123, "Bird Tweet"},
  {0, 0, 124, "Telephone Ring"},
  {0, 0, 125, "Helicopter"},
  {0, 0, 126, "Applause"},
  {0, 0, 127, "Gunshot"},
};

// ── Drum Kits (always CH10, CC0=0, CC32=0) ────────────────────────────────────
// PC 0=GM, 8=Power, 16=Analog, 24=Brush (only these 4 are valid on AI202)
static const SoundEntry DRUM_KITS[] PROGMEM = {
  {0, 0,  0, "GM Drum Kit"},
  {0, 0,  8, "Power Kit"},
  {0, 0, 16, "Analog Kit"},
  {0, 0, 24, "Brush Kit"},
};

// ── Korg Variation Bank (CC0=0, CC32=1) ───────────────────────────────────────
// GS-style variations on selected GM programs.
// Only the most commonly available ones on AI202 / AG-10.
// Select using CC0=0, CC32=1, then PC = same number as GM counterpart.
// Falls back to GM sound if variation not present.
static const SoundEntry VAR_SOUNDS[] PROGMEM = {
  {0, 1,   0, "Wide Grand"},
  {0, 1,   1, "Dark Grand"},
  {0, 1,   4, "Detuned EP1"},
  {0, 1,   5, "Detuned EP2"},
  {0, 1,  16, "Detuned Organ"},
  {0, 1,  19, "Chapel Organ"},
  {0, 1,  24, "Ukulele"},
  {0, 1,  25, "12-String Gtr"},
  {0, 1,  26, "Hawaii Guitar"},
  {0, 1,  28, "Funk Guitar"},
  {0, 1,  30, "Feedback Guitar"},
  {0, 1,  32, "Fingered Bass 2"},
  {0, 1,  38, "Warm Synth Bass"},
  {0, 1,  39, "Synth Bass 3"},
  {0, 1,  48, "Orchestra"},
  {0, 1,  50, "Synth Strings 3"},
  {0, 1,  52, "Choir 2"},
  {0, 1,  56, "Trumpet 2"},
  {0, 1,  62, "Synth Brass 3"},
  {0, 1,  80, "Sine Lead"},
  {0, 1,  88, "Fantasia Pad"},
  {0, 1,  89, "Warm Pad 2"},
};

// ── Custom User Patch (Bank 62) ────────────────────────────────────────────────
// CC0=62, CC32=0, PC=96 (0-based)
// This slot holds a SysEx-programmed custom AI202 patch.
// Name is runtime-configurable (stored in EEPROM or just in RAM).
// See SYSEX.md for the AI202 patch programming format.
static const SoundEntry USR_PATCH PROGMEM = {62, 0, 96, "User Patch"};

// ── Bank descriptor table ─────────────────────────────────────────────────────
enum class BankID : uint8_t {
  GM  = 0,
  VAR = 1,
  USR = 2,
  DRM = 3,
};

struct BankDesc {
  BankID      id;
  uint8_t     cc0;
  uint8_t     cc32;
  uint8_t     count;
  const char* label;
};

static const BankDesc BANKS[] PROGMEM = {
  { BankID::GM,  0,  0, 128, "GM"  },
  { BankID::VAR, 0,  1,  22, "VAR" },
  { BankID::USR, 62, 0,   1, "USR" },
  { BankID::DRM, 0,  0,   4, "DRM" },
};
static const uint8_t BANK_COUNT = 4;

// ── Lookup helpers ─────────────────────────────────────────────────────────────
// Returns pointer to name string in PROGMEM.
// Caller must use strcpy_P or similar to read it.
inline const char* getSoundName(BankID bank, uint8_t idx) {
  switch (bank) {
    case BankID::GM:
      if (idx < 128) return GM_SOUNDS[idx].name;
      break;
    case BankID::VAR:
      if (idx < 22)  return VAR_SOUNDS[idx].name;
      break;
    case BankID::DRM:
      if (idx < 4)   return DRUM_KITS[idx].name;
      break;
    case BankID::USR:
      return USR_PATCH.name;
  }
  return "---";
}

inline const SoundEntry* getSoundEntry(BankID bank, uint8_t idx) {
  switch (bank) {
    case BankID::GM:  if (idx < 128) return &GM_SOUNDS[idx];  break;
    case BankID::VAR: if (idx < 22)  return &VAR_SOUNDS[idx]; break;
    case BankID::DRM: if (idx < 4)   return &DRUM_KITS[idx];  break;
    case BankID::USR: return &USR_PATCH;
  }
  return nullptr;
}
