#include <Arduino.h>

extern "C" void rr_unity_putc(int c) {
  Serial.write((uint8_t)c);
}

extern "C" void rr_unity_flush(void) { Serial.flush(); }
