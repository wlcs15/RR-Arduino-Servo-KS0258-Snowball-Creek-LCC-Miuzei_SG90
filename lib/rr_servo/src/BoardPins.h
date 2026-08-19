#ifndef RR_SERVO_BOARD_PINS_H
#define RR_SERVO_BOARD_PINS_H

// Pin map for Snowball Creek LCC shield + KS0258.
// Override any symbol before including this header.

#ifndef RR_PCA9685_ADDR
#define RR_PCA9685_ADDR 0x40
#endif

#ifndef RR_LIMIT_PIN_0
#define RR_LIMIT_PIN_0 A0
#endif

#ifndef RR_SERVO_CH_0
#define RR_SERVO_CH_0 0
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

#endif
