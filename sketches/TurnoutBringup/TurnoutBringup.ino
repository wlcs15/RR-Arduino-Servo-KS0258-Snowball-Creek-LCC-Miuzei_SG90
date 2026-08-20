// Mega 2560 bring-up: 16 KS0258 channels.
// DEBUG is off. Rest position 45 deg. At startup all channels sweep
// 45 -> 135 in ~3 s (Tortoise-like), then 135 -> 45 in ~3 s, then hold 45.
// -DRR_USE_KS0258=0 uses Servo on D44-D46 (three turnouts).

#ifdef DEBUG
#undef DEBUG
#endif

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

static const unsigned kRestDeg = 45u;
static const unsigned kFarDeg = 135u;
static const uint32_t kSweepMs = 3000u;

static LimitLadderConfig ladderCfg = limit_ladder_default_10bit();
static LimitLadderFilter ladderFilter[RR_TURNOUT_COUNT];
static TurnoutChannel turnout[RR_TURNOUT_COUNT];
static int lastRaw[RR_TURNOUT_COUNT];
static int lastAvg[RR_TURNOUT_COUNT];
static uint32_t lastStatusMs = 0;
static unsigned selected = 0;
static uint16_t restUs = (uint16_t)RR_SG90_US_90;

static uint16_t sg90_us_from_deg(unsigned deg) {
  if (deg > 180u) {
    deg = 180u;
  }
  return (uint16_t)((uint32_t)RR_SG90_US_0 +
                    ((uint32_t)(RR_SG90_US_180 - RR_SG90_US_0) * (uint32_t)deg) /
                        180u);
}

#if RR_USE_KS0258
static void write_all_us(uint16_t us) {
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    pwm.writeMicroseconds((uint8_t)i, us);
  }
}

static uint16_t lerp_us(uint16_t fromUs, uint16_t toUs, uint32_t elapsed,
                        uint32_t duration) {
  if (elapsed >= duration) {
    return toUs;
  }
  const int32_t delta = (int32_t)toUs - (int32_t)fromUs;
  return (uint16_t)((int32_t)fromUs + (delta * (int32_t)elapsed) / (int32_t)duration);
}

static void sweep_all(uint16_t fromUs, uint16_t toUs, uint32_t durationMs) {
  const uint32_t t0 = millis();
  for (;;) {
    const uint32_t elapsed = millis() - t0;
    const uint16_t us = lerp_us(fromUs, toUs, elapsed, durationMs);
    write_all_us(us);
    Serial.print(F("sweep_us="));
    Serial.println(us);
    if (elapsed >= durationMs) {
      break;
    }
    delay(15);
  }
  write_all_us(toUs);
}
#endif

static void drive_one(unsigned idx) {
  if (idx >= (unsigned)RR_TURNOUT_COUNT) {
    return;
  }
  if (!turnout[idx].drive_enabled()) {
#if RR_USE_KS0258
    pwm.writeMicroseconds((uint8_t)idx, restUs);
#else
    if (!fallbackServo[idx].attached()) {
      fallbackServo[idx].attach(rr_fallback_pwm_pin(idx));
    }
    fallbackServo[idx].writeMicroseconds(restUs);
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
  Serial.print(F(" rest_us="));
  Serial.print(restUs);
  Serial.print(F(" us="));
  Serial.println(turnout[idx].pulse_us());
}

static void help() {
  Serial.println(F("TurnoutBringup Mega (DEBUG off, rest 45 deg)"));
  Serial.println(RR_I2C_NOTE);
  Serial.print(F("RR_USE_KS0258="));
  Serial.println(RR_USE_KS0258);
  Serial.print(F("rest_us="));
  Serial.println(restUs);
  Serial.println(F("n/p  select channel"));
  Serial.println(F("t    throw selected"));
  Serial.println(F("c    close selected"));
  Serial.println(F("s    status all"));
  Serial.println(F("Do not t/c until limit ladder is wired"));
}

static char cmd_key(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static void next_channel() {
  selected = (selected + 1u) % (unsigned)RR_TURNOUT_COUNT;
  Serial.print(F("sel="));
  Serial.println(selected);
}

static void prev_channel() {
  selected = (selected + (unsigned)RR_TURNOUT_COUNT - 1u) %
             (unsigned)RR_TURNOUT_COUNT;
  Serial.print(F("sel="));
  Serial.println(selected);
}

static void status_all() {
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    print_one(i);
  }
}

static void handle_serial(uint32_t nowMs) {
  if (!Serial.available()) {
    return;
  }
  const char key = cmd_key((char)Serial.read());
  if (key == 'n') {
    next_channel();
  } else if (key == 'p') {
    prev_channel();
  } else if (key == 't') {
    turnout[selected].command(TURNOUT_CMD_THROWN, nowMs);
  } else if (key == 'c') {
    turnout[selected].command(TURNOUT_CMD_CLOSED, nowMs);
  } else if (key == 's') {
    status_all();
  } else if (key == 'h' || key == '?') {
    help();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }

  restUs = sg90_us_from_deg(kRestDeg);

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

  const uint16_t farUs = sg90_us_from_deg(kFarDeg);
  Serial.println(F("startup: hold 45 deg"));
  write_all_us(restUs);
  delay(400);
  Serial.println(F("startup: 45 -> 135 in 3 s (Tortoise-like)"));
  sweep_all(restUs, farUs, kSweepMs);
  delay(300);
  Serial.println(F("startup: 135 -> 45 in 3 s"));
  sweep_all(farUs, restUs, kSweepMs);
  Serial.println(F("startup: hold 45 deg"));
  write_all_us(restUs);
#endif

  help();
}

void loop() {
  const uint32_t now = millis();
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    sample_one(i, now);
  }
  handle_serial(now);
  if ((now - lastStatusMs) >= 1000u) {
    lastStatusMs = now;
    print_one(selected);
  }
}
