// Mega 2560 bring-up: up to 16 KS0258 channels, analog ladders on A0–A3
// and A6–A15 (channels 14–15 drive only).
// -DRR_USE_KS0258=0 uses Servo on D44–D46 (three turnouts).

#include <Wire.h>

#include <BoardPins.h>
#include <LimitLadder.h>
#include <TurnoutChannel.h>

#if RR_USE_KS0258
#include <Adafruit_PWMServoDriver.h>
static Adafruit_PWMServoDriver pwm(RR_PCA9685_ADDR);
#else
#include <Servo.h>
static Servo fallbackServo[RR_TURNOUT_COUNT];
#endif

static LimitLadderConfig ladderCfg = limit_ladder_default_10bit();
static LimitLadderFilter ladderFilter[RR_TURNOUT_COUNT];
static TurnoutChannel turnout[RR_TURNOUT_COUNT];
static int lastRaw[RR_TURNOUT_COUNT];
static int lastAvg[RR_TURNOUT_COUNT];
static uint32_t lastStatusMs = 0;
static unsigned selected = 0;

static void drive_one(unsigned idx) {
  if (idx >= (unsigned)RR_TURNOUT_COUNT) {
    return;
  }
  if (!turnout[idx].drive_enabled()) {
#if RR_USE_KS0258
    pwm.setPWM((uint8_t)idx, 0, 0);
#else
    fallbackServo[idx].detach();
#endif
    return;
  }
#if RR_USE_KS0258
  pwm.writeMicroseconds((uint8_t)idx, turnout[idx].pulse_us());
#else
  if (!fallbackServo[idx].attached()) {
    fallbackServo[idx].attach(rr_fallback_pwm_pin(idx));
  }
  fallbackServo[idx].writeMicroseconds(turnout[idx].pulse_us());
#endif
}

static void sample_one(unsigned idx, uint32_t nowMs) {
  const int pin = rr_limit_pin(idx);
  if (pin < 0) {
    lastRaw[idx] = 1023;
    lastAvg[idx] = 1023;
    turnout[idx].update(nowMs, LIMIT_NEITHER);
    drive_one(idx);
    return;
  }
  lastRaw[idx] = analogRead(pin);
  lastAvg[idx] = ladderFilter[idx].push(lastRaw[idx]);
  turnout[idx].update(nowMs, limit_ladder_decode(lastAvg[idx], ladderCfg));
  drive_one(idx);
}

static void print_one(unsigned idx) {
  Serial.print(F("ch="));
  Serial.print(idx);
  Serial.print(F(" adc="));
  Serial.print(lastRaw[idx]);
  Serial.print(F(" avg="));
  Serial.print(lastAvg[idx]);
  Serial.print(F(" limit="));
  Serial.print(limit_state_name(turnout[idx].last_limit()));
  Serial.print(F(" motion="));
  Serial.print(turnout[idx].motion_name());
  Serial.print(F(" fault="));
  Serial.print(turnout[idx].fault_name());
  Serial.print(F(" us="));
  Serial.println(turnout[idx].pulse_us());
}

static void help() {
  Serial.println(F("TurnoutBringup Mega"));
  Serial.println(RR_I2C_NOTE);
  Serial.print(F("RR_USE_KS0258="));
  Serial.println(RR_USE_KS0258);
  Serial.print(F("count="));
  Serial.println(RR_TURNOUT_COUNT);
  Serial.println(F("n/p  select channel"));
  Serial.println(F("t    throw selected"));
  Serial.println(F("c    close selected"));
  Serial.println(F("s    status all"));
  Serial.println(F("z    release PWM selected"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }

  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    const int pin = rr_limit_pin(i);
    if (pin >= 0) {
      pinMode(pin, INPUT);
    }
    turnout[i].set_config(turnout_default_sg90());
  }

#if RR_USE_KS0258
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    pwm.setPWM((uint8_t)i, 0, 0);
  }
#endif

  help();
}

void loop() {
  const uint32_t now = millis();
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    sample_one(i, now);
  }

  if (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == 'n' || ch == 'N') {
      selected = (selected + 1u) % (unsigned)RR_TURNOUT_COUNT;
      Serial.print(F("sel="));
      Serial.println(selected);
    } else if (ch == 'p' || ch == 'P') {
      selected = (selected + (unsigned)RR_TURNOUT_COUNT - 1u) %
                 (unsigned)RR_TURNOUT_COUNT;
      Serial.print(F("sel="));
      Serial.println(selected);
    } else if (ch == 't' || ch == 'T') {
      turnout[selected].command(TURNOUT_CMD_THROWN, now);
    } else if (ch == 'c' || ch == 'C') {
      turnout[selected].command(TURNOUT_CMD_CLOSED, now);
    } else if (ch == 's' || ch == 'S') {
      for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
        print_one(i);
      }
    } else if (ch == 'z' || ch == 'Z') {
#if RR_USE_KS0258
      pwm.setPWM((uint8_t)selected, 0, 0);
#else
      fallbackServo[selected].detach();
#endif
    } else if (ch == 'h' || ch == 'H' || ch == '?') {
      help();
    }
  }

  if ((now - lastStatusMs) >= 1000u) {
    lastStatusMs = now;
    print_one(selected);
  }
}
