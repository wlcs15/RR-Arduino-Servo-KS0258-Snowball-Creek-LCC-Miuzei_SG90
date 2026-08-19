#ifndef RR_SERVO_BOARD_PINS_H
#define RR_SERVO_BOARD_PINS_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Pin map for Snowball Creek LCC shield + KS0258 (Mega trunk).
// Override any symbol before including this header.

#ifndef RR_USE_KS0258
#define RR_USE_KS0258 1
#endif

#ifndef RR_PCA9685_ADDR
#define RR_PCA9685_ADDR 0x40
#endif

#if RR_USE_KS0258
#ifndef RR_TURNOUT_COUNT
#define RR_TURNOUT_COUNT 16
#endif
#else
#ifndef RR_TURNOUT_COUNT
#define RR_TURNOUT_COUNT 3
#endif
#endif

#ifndef RR_LIMIT_PIN_0
#define RR_LIMIT_PIN_0 A0
#endif

#ifndef RR_SERVO_CH_0
#define RR_SERVO_CH_0 0
#endif

#ifndef RR_FALLBACK_PWM_0
#define RR_FALLBACK_PWM_0 44
#endif
#ifndef RR_FALLBACK_PWM_1
#define RR_FALLBACK_PWM_1 45
#endif
#ifndef RR_FALLBACK_PWM_2
#define RR_FALLBACK_PWM_2 46
#endif

// Snowball Creek (do not reuse in application code).
#ifndef RR_LCC_MCP2515_CS
#define RR_LCC_MCP2515_CS 8
#endif
#ifndef RR_LCC_MCP2515_IRQ
#define RR_LCC_MCP2515_IRQ 2
#endif
#ifndef RR_LCC_EEPROM_CS
#define RR_LCC_EEPROM_CS 7
#endif
#ifndef RR_LCC_DCC_PIN
#define RR_LCC_DCC_PIN 3
#endif

#if defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_MEGA)
#define RR_BOARD_MEGA 1
#define RR_I2C_NOTE "Mega: jumper KS0258 A4->20 and A5->21, then use Wire"
#else
#define RR_BOARD_MEGA 0
#define RR_I2C_NOTE "Uno/Nano: KS0258 uses A4/A5 hardware I2C"
#endif

#ifdef ARDUINO
inline int rr_limit_pin(unsigned idx) {
#if RR_BOARD_MEGA
  static const int kPins[16] = {A0,  A1,  A2,  A3,  A6,  A7,  A8,  A9,
                                A10, A11, A12, A13, A14, A15, -1,  -1};
#else
  static const int kPins[16] = {A0, A1, A2, A3, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1};
#endif
  if (idx >= 16u) {
    return -1;
  }
  return kPins[idx];
}

inline int rr_fallback_pwm_pin(unsigned idx) {
  static const int kPins[3] = {RR_FALLBACK_PWM_0, RR_FALLBACK_PWM_1,
                               RR_FALLBACK_PWM_2};
  if (idx >= 3u) {
    return -1;
  }
  return kPins[idx];
}
#else
inline int rr_limit_pin(unsigned idx) {
  (void)idx;
  return -1;
}
inline int rr_fallback_pwm_pin(unsigned idx) {
  (void)idx;
  return -1;
}
#endif

#endif
