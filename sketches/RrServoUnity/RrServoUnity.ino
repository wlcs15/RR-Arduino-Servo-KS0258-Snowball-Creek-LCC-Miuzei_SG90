// On-target Unity for Mega 2560. No servo PWM. Same tests as host.
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
  Serial.println(F("RrServoUnity Mega 2560"));
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
