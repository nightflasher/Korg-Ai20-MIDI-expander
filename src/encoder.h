#pragma once
/*
 * encoder.h  -  3x Bare Rotary Encoder Handler
 *
 * Encoder 1 (ENC1): Navigation  - bewegt den Menü-Cursor durch Parameter
 * Encoder 2 (ENC2): Programm    - aendert Programm/Bank direkt, kein Edit-Mode noetig
 * Encoder 3 (ENC3): Volume      - aendert Lautstaerke des aktiven Kanals direkt
 *
 * Alle Encoder ohne Board -> externe 10kOhm Pull-ups an CLK und DT nach 3.3V Pflicht.
 * SW-Pins nutzen internen Pull-up des ESP32-S3.
 *
 * Jeder Encoder hat einen unabhaengigen Delta-Zaehler und Pressed-Flag,
 * die im Loop atomar ausgelesen und resettet werden.
 */

#include <Arduino.h>
#include "pins.h"

struct EncoderState {
  volatile int  delta   = 0;
  volatile bool pressed = false;
  uint8_t       last    = 0;
};

static EncoderState enc[3];

// ── ISRs ──────────────────────────────────────────────────────────────────────
void IRAM_ATTR enc1RotISR() {
  uint8_t a=digitalRead(PIN_ENC1_CLK), b=digitalRead(PIN_ENC1_DT);
  if(a!=enc[0].last){ enc[0].delta+=(b!=a)?1:-1; enc[0].last=a; }
}
void IRAM_ATTR enc1BtnISR() { enc[0].pressed=true; }

void IRAM_ATTR enc2RotISR() {
  uint8_t a=digitalRead(PIN_ENC2_CLK), b=digitalRead(PIN_ENC2_DT);
  if(a!=enc[1].last){ enc[1].delta+=(b!=a)?1:-1; enc[1].last=a; }
}
void IRAM_ATTR enc2BtnISR() { enc[1].pressed=true; }

void IRAM_ATTR enc3RotISR() {
  uint8_t a=digitalRead(PIN_ENC3_CLK), b=digitalRead(PIN_ENC3_DT);
  if(a!=enc[2].last){ enc[2].delta+=(b!=a)?1:-1; enc[2].last=a; }
}
void IRAM_ATTR enc3BtnISR() { enc[2].pressed=true; }

// ── Init ──────────────────────────────────────────────────────────────────────
void encodersBegin() {
  // CLK + DT: externe Pull-ups vorhanden, daher INPUT (nicht INPUT_PULLUP)
  const uint8_t clkPins[] = {PIN_ENC1_CLK, PIN_ENC2_CLK, PIN_ENC3_CLK};
  const uint8_t dtPins[]  = {PIN_ENC1_DT,  PIN_ENC2_DT,  PIN_ENC3_DT};
  const uint8_t swPins[]  = {PIN_ENC1_SW,  PIN_ENC2_SW,  PIN_ENC3_SW};

  for(int i=0;i<3;i++){
    pinMode(clkPins[i], INPUT);
    pinMode(dtPins[i],  INPUT);
    pinMode(swPins[i],  INPUT_PULLUP);
  }

  enc[0].last = digitalRead(PIN_ENC1_CLK);
  enc[1].last = digitalRead(PIN_ENC2_CLK);
  enc[2].last = digitalRead(PIN_ENC3_CLK);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC1_CLK), enc1RotISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC1_SW),  enc1BtnISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC2_CLK), enc2RotISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC2_SW),  enc2BtnISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC3_CLK), enc3RotISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC3_SW),  enc3BtnISR, FALLING);
}

// ── Atomic read & reset ───────────────────────────────────────────────────────
// Gibt Delta und Pressed-Status fuer Encoder i (0-2) zurueck und resettet.
inline int  encDelta(uint8_t i)   { noInterrupts(); int d=enc[i].delta;   enc[i].delta=0;   interrupts(); return d; }
inline bool encPressed(uint8_t i) { noInterrupts(); bool p=enc[i].pressed; enc[i].pressed=false; interrupts(); return p; }
