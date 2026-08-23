#ifndef RR_SERVO_BOARD_PINS_H
#define RR_SERVO_BOARD_PINS_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Pin map. Mega trunk: Snowball Creek + KS0258.
// wemos-d1r32: new features #if defined(ARDUINO_ARCH_ESP32) only.
// CAN shield removed for this node: PCA9685 module on default I2C
// GPIO21/22 (silk SDA/SCL). 4" TFT uses D3-D13 only — those I2C pins stay.
// A4/A5 are GPIO36/39 input-only (not I2C). Ladder A1. Node .A5.03.
// Override before including.

#ifdef ARDUINO_ARCH_ESP32
#ifndef RR_USE_KS0258
#define RR_USE_KS0258 0
#endif
#ifndef RR_TURNOUT_COUNT
#define RR_TURNOUT_COUNT 1
#endif
#ifndef RR_LIMIT_PIN_0
#define RR_LIMIT_PIN_0 A1
#endif
#ifndef RR_FALLBACK_PWM_0
#define RR_FALLBACK_PWM_0 25
#endif
#ifndef RR_FALLBACK_PWM_1
#define RR_FALLBACK_PWM_1 17
#endif
#ifndef RR_FALLBACK_PWM_2
#define RR_FALLBACK_PWM_2 16
#endif
#ifndef RR_OWLTHREE_NODE_ID
#define RR_OWLTHREE_NODE_ID 0x05010101A503ULL
#endif
#ifndef RR_ESP32_I2C_SDA
#define RR_ESP32_I2C_SDA 21
#endif
#ifndef RR_ESP32_I2C_SCL
#define RR_ESP32_I2C_SCL 22
#endif
#endif

#ifndef RR_USE_KS0258
#define RR_USE_KS0258 1
#endif

#ifndef RR_OWLTHREE_NODE_ID
#define RR_OWLTHREE_NODE_ID 0x05010101A502ULL
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
#elif defined(ARDUINO_ARCH_ESP32)
#define RR_BOARD_MEGA 0
#define RR_I2C_NOTE "D1 R32: PCA9685 SDA GPIO21 SCL GPIO22; analog A1; TFT keeps D3-D13"
#else
#define RR_BOARD_MEGA 0
#define RR_I2C_NOTE "Uno/Nano: KS0258 uses A4/A5 hardware I2C"
#endif

#ifdef ARDUINO
inline int rr_limit_pin(unsigned idx) {
#if RR_BOARD_MEGA
  static const int kPins[16] = {A0,  A1,  A2,  A3,  A6,  A7,  A8,  A9,
                                A10, A11, A12, A13, A14, A15, -1,  -1};
#elif defined(ARDUINO_ARCH_ESP32)
  static const int kPins[16] = {A1, A2, A3, A4, -1, -1, -1, -1,
                                -1, -1, -1, -1, -1, -1, -1, -1};
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
