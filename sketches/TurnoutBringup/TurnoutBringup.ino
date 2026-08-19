// Serial bring-up for one Miuzei SG90 on KS0258 channel 0
// and a dual-limit resistor ladder on A0.
//
// Libraries: Adafruit PWM Servo Driver Library (and Adafruit BusIO).
// Copy ../../lib/rr_servo into Arduino/libraries/ or add the src/ files
// to this sketch folder.

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#include <BoardPins.h>
#include <LimitLadder.h>
#include <TurnoutChannel.h>

static Adafruit_PWMServoDriver pwm(RR_PCA9685_ADDR);
static LimitLadderConfig ladderCfg = limit_ladder_default_10bit();
static LimitLadderFilter ladderFilter(4);
static TurnoutChannel turnout;

static int lastRaw = 0;
static int lastAvg = 0;
static uint32_t lastStatusMs = 0;

static void apply_pwm() {
  if (turnout.drive_enabled()) {
    pwm.writeMicroseconds(RR_SERVO_CH_0, turnout.pulse_us());
  } else {
    pwm.setPWM(RR_SERVO_CH_0, 0, 0);
  }
}

static void print_status() {
  Serial.print(F("adc="));
  Serial.print(lastRaw);
  Serial.print(F(" avg="));
  Serial.print(lastAvg);
  Serial.print(F(" limit="));
  Serial.print(limit_state_name(turnout.last_limit()));
  Serial.print(F(" motion="));
  Serial.print(turnout.motion_name());
  Serial.print(F(" fault="));
  Serial.print(turnout.fault_name());
  Serial.print(F(" drive="));
  Serial.print(turnout.drive_enabled() ? 1 : 0);
  Serial.print(F(" us="));
  Serial.println(turnout.pulse_us());
}

static void help() {
  Serial.println(F("TurnoutBringup"));
  Serial.println(RR_I2C_NOTE);
  Serial.println(F("t  throw"));
  Serial.println(F("c  close"));
  Serial.println(F("s  status"));
  Serial.println(F("z  release PWM"));
  Serial.println(F("h  help"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }

  pinMode(RR_LIMIT_PIN_0, INPUT);

  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
  pwm.setPWM(RR_SERVO_CH_0, 0, 0);

  TurnoutConfig cfg = turnout_default_sg90();
  turnout.set_config(cfg);

  help();
}

void loop() {
  lastRaw = analogRead(RR_LIMIT_PIN_0);
  lastAvg = ladderFilter.push(lastRaw);
  turnout.update(millis(), limit_ladder_decode(lastAvg, ladderCfg));
  apply_pwm();

  if (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == 't' || ch == 'T') {
      turnout.command(TURNOUT_CMD_THROWN, millis());
      Serial.println(F("cmd thrown"));
    } else if (ch == 'c' || ch == 'C') {
      turnout.command(TURNOUT_CMD_CLOSED, millis());
      Serial.println(F("cmd closed"));
    } else if (ch == 's' || ch == 'S') {
      print_status();
    } else if (ch == 'z' || ch == 'Z') {
      pwm.setPWM(RR_SERVO_CH_0, 0, 0);
      Serial.println(F("pwm off"));
    } else if (ch == 'h' || ch == 'H' || ch == '?') {
      help();
    }
  }

  uint32_t now = millis();
  if ((now - lastStatusMs) >= 500) {
    lastStatusMs = now;
    print_status();
  }
}
