// Mega 2560 + Snowball Creek pins + KS0258.
//
// LCC on Mega is intentionally not linked to libLCC (GPL-2.0).
// Application license is BSD-2-Clause; ESP32/ARM nodes will use OpenMRN.
// This sketch still uses the Snowball Creek CS/IRQ pins so a later BSD
// OpenLCB AVR port can drop in. Until then, serial 't'/'c' drives the servo.
//
// Planned event map for turnout 0, node 05.01.01.01.A5.02:
//   consume  ...:00 throw
//   consume  ...:01 close
//   produce  ...:10 neither
//   produce  ...:11 thrown limit
//   produce  ...:12 closed limit
//   produce  ...:13 both (fault)

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PWMServoDriver.h>

#include <BoardPins.h>
#include <LimitLadder.h>
#include <TurnoutChannel.h>

#if __has_include(<ACAN2515.h>)
#define RR_HAS_ACAN 1
#include <ACAN2515.h>
#else
#define RR_HAS_ACAN 0
#endif

static Adafruit_PWMServoDriver pwm(RR_PCA9685_ADDR);
static LimitLadderConfig ladderCfg = limit_ladder_default_10bit();
static LimitLadderFilter ladderFilter(4);
static TurnoutChannel turnout;
static LimitState lastAnnounced = LIMIT_NEITHER;
static bool announcedInit = false;

#if RR_HAS_ACAN
static ACAN2515 can(RR_LCC_MCP2515_CS, SPI, RR_LCC_MCP2515_IRQ);
#endif

static void apply_pwm() {
  if (turnout.drive_enabled()) {
    pwm.writeMicroseconds(RR_SERVO_CH_0, turnout.pulse_us());
  } else {
    pwm.setPWM(RR_SERVO_CH_0, 0, 0);
  }
}

static void handle_command(TurnoutCommand cmd) {
  turnout.command(cmd, millis());
}

static char cmd_key(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static void maybe_announce_limit(LimitState lim) {
  if (announcedInit && lim == lastAnnounced) {
    return;
  }
  lastAnnounced = lim;
  announcedInit = true;
  Serial.print(F("limit-event "));
  Serial.println(limit_state_name(lim));
}

static void print_status(LimitState lim) {
  Serial.print(F("limit="));
  Serial.print(limit_state_name(lim));
  Serial.print(F(" motion="));
  Serial.println(turnout.motion_name());
}

static void handle_serial(LimitState lim) {
  if (!Serial.available()) {
    return;
  }
  const char key = cmd_key((char)Serial.read());
  if (key == 't') {
    handle_command(TURNOUT_CMD_THROWN);
  } else if (key == 'c') {
    handle_command(TURNOUT_CMD_CLOSED);
  } else if (key == 's') {
    print_status(lim);
  }
}

// OpenLCB event I/O belongs here once a BSD-licensed AVR stack is chosen.

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }

  pinMode(RR_LIMIT_PIN_0, INPUT);
  pinMode(RR_LCC_EEPROM_CS, OUTPUT);
  digitalWrite(RR_LCC_EEPROM_CS, HIGH);

  SPI.begin();
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
  pwm.setPWM(RR_SERVO_CH_0, 0, 0);
  turnout.set_config(turnout_default_sg90());

#if RR_HAS_ACAN
  ACAN2515Settings settings(8 * 1000 * 1000, 125 * 1000);  // 8 MHz xtal, LCC 125 kbit/s
  const uint16_t err = can.begin(settings, [] { can.isr(); });
  Serial.print(F("ACAN2515 begin="));
  Serial.println(err);
#else
  Serial.println(F("ACAN2515 not installed; serial control only"));
#endif

  Serial.println(F("LCC events not linked (BSD node; OpenMRN on ESP32/ARM)"));

  Serial.println(F("LccTurnoutNode  t=throw  c=close  s=status"));
}

void loop() {
  const int raw = analogRead(RR_LIMIT_PIN_0);
  const int avg = ladderFilter.push(raw);
  const LimitState lim = limit_ladder_decode(avg, ladderCfg);
  turnout.update(millis(), lim);
  apply_pwm();
  maybe_announce_limit(lim);
  handle_serial(lim);
#if RR_HAS_ACAN
  can.poll();
#endif
}
