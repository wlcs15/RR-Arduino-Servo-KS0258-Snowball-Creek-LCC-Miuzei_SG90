// On-target Unity. No servo PWM and no CAN. Same tests as host.
// Does not attach D9/D10/SDA/SCL so KS0258, Adafruit 1438, and Waveshare
// RS485 CAN (B00XMERZA4) headers stay electrically free.
#ifdef DEBUG
#undef DEBUG
#endif

#include "unity.h"

int rr_servo_run_unity_tests(void);
#if defined(RR_GCOV)
extern "C" void __gcov_dump(void);
#endif

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
  }
  delay(200);
#if defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("RrServoUnity D1 R32"));
#else
  Serial.println(F("RrServoUnity Mega 2560"));
#endif
#if defined(RR_GCOV)
  Serial.println(F("gcov=on"));
#endif
  const int fail = rr_servo_run_unity_tests();
  Serial.print(F("unity_failures="));
  Serial.println(fail);
#if defined(RR_GCOV)
  __gcov_dump();
  Serial.println(F("gcov_dump_done"));
#endif
}

void loop() {}
