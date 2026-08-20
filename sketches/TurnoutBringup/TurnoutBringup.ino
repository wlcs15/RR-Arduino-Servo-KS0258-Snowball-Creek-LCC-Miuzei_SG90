// Mega 2560 bring-up: 16 KS0258 channels.
// Compile with -DDEBUG (or -DRR_DEBUG): hold all channels at 90 deg
// (1500 us). Extra serial. No 45-135 sweep.
// DEBUG off: repeating cycle 45 -> 135 -> 45, then return to 90 before
// starting the cycle again. Idle rest is 90 deg.
// -DRR_USE_KS0258=0 uses Servo on D44-D46 (three turnouts).

#if defined(DEBUG) || defined(RR_DEBUG)
#define RR_BRINGUP_DEBUG 1
#ifdef DEBUG
#undef DEBUG
#endif
#else
#define RR_BRINGUP_DEBUG 0
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

static const unsigned kNearDeg = 45u;
static const unsigned kMidDeg = 90u;
static const unsigned kFarDeg = 135u;
static const uint32_t kSweepMs = 3000u;
static const uint32_t kHalfSweepMs = 1500u;
static const uint32_t kHoldMidMs = 400u;

enum {
  kDemoToFar = 0,
  kDemoToNear,
  kDemoToMid,
  kDemoHoldMid,
  kDemoMidToNear
};

static LimitLadderConfig ladderCfg = limit_ladder_default_10bit();
static LimitLadderFilter ladderFilter[RR_TURNOUT_COUNT];
static TurnoutChannel turnout[RR_TURNOUT_COUNT];
static int lastRaw[RR_TURNOUT_COUNT];
static int lastAvg[RR_TURNOUT_COUNT];
static uint32_t lastStatusMs = 0;
static unsigned selected = 0;
static uint16_t restUs = (uint16_t)RR_SG90_US_90;
static uint16_t nearUs = (uint16_t)RR_SG90_US_0;
static uint16_t midUs = (uint16_t)RR_SG90_US_90;
static uint16_t farUs = (uint16_t)RR_SG90_US_180;
static uint16_t holdUs = (uint16_t)RR_SG90_US_90;
static uint8_t demoPhase = kDemoToFar;
static uint32_t demoT0 = 0;
static uint16_t demoFromUs = (uint16_t)RR_SG90_US_90;
static uint16_t demoToUs = (uint16_t)RR_SG90_US_90;
static uint32_t demoDurMs = kSweepMs;
static uint32_t lastSweepPrintMs = 0;

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

#if !RR_BRINGUP_DEBUG
static void demo_begin_phase(uint8_t phase) {
  demoPhase = phase;
  demoT0 = millis();
  lastSweepPrintMs = 0;
  if (phase == kDemoToFar) {
    demoFromUs = nearUs;
    demoToUs = farUs;
    demoDurMs = kSweepMs;
    Serial.println(F("cycle: 45 -> 135 in 3 s"));
  } else if (phase == kDemoToNear) {
    demoFromUs = farUs;
    demoToUs = nearUs;
    demoDurMs = kSweepMs;
    Serial.println(F("cycle: 135 -> 45 in 3 s"));
  } else if (phase == kDemoToMid) {
    demoFromUs = nearUs;
    demoToUs = midUs;
    demoDurMs = kHalfSweepMs;
    Serial.println(F("cycle: 45 -> 90 in 1.5 s"));
  } else if (phase == kDemoHoldMid) {
    demoFromUs = midUs;
    demoToUs = midUs;
    demoDurMs = kHoldMidMs;
    Serial.println(F("cycle: hold 90 deg"));
    write_all_us(midUs);
    holdUs = midUs;
  } else {
    demoFromUs = midUs;
    demoToUs = nearUs;
    demoDurMs = kHalfSweepMs;
    Serial.println(F("cycle: 90 -> 45 in 1.5 s"));
  }
}

static void demo_next_phase(void) {
  uint8_t next = (uint8_t)(demoPhase + 1u);
  if (next > kDemoMidToNear) {
    next = kDemoToFar;
  }
  demo_begin_phase(next);
}

static void demo_tick(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - demoT0;
  if (demoPhase == kDemoHoldMid) {
    write_all_us(midUs);
    holdUs = midUs;
    if (elapsed >= demoDurMs) {
      demo_next_phase();
    }
    return;
  }
  const uint16_t us = lerp_us(demoFromUs, demoToUs, elapsed, demoDurMs);
  write_all_us(us);
  holdUs = us;
#ifdef HACK
  if ((lastSweepPrintMs == 0) || ((nowMs - lastSweepPrintMs) >= 15u)) {
    lastSweepPrintMs = nowMs;
    Serial.print(F("sweep_us="));
    Serial.println(us);
  }
#endif
  if (elapsed >= demoDurMs) {
    write_all_us(demoToUs);
    holdUs = demoToUs;
    demo_next_phase();
  }
}

static bool any_drive_enabled(void) {
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    if (turnout[i].drive_enabled()) {
      return true;
    }
  }
  return false;
}
#endif
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

static void sample_one(unsigned idx, uint32_t nowMs, bool write_pwm) {
  const int pin = rr_limit_pin(idx);
  if (pin < 0) {
    lastRaw[idx] = 1023;
    lastAvg[idx] = 1023;
    turnout[idx].update(nowMs, LIMIT_NEITHER);
    if (write_pwm) {
      drive_one(idx);
    }
    return;
  }
  lastRaw[idx] = analogRead(pin);
  lastAvg[idx] = ladderFilter[idx].push(lastRaw[idx]);
  turnout[idx].update(nowMs, limit_ladder_decode(lastAvg[idx], ladderCfg));
  if (write_pwm) {
    drive_one(idx);
  }
}

#if RR_USE_KS0258
static void startup_pwm() {
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
#if RR_BRINGUP_DEBUG
  Serial.println(F("startup: DEBUG hold 90 deg (1500 us), no sweep"));
  write_all_us(restUs);
  holdUs = restUs;
#else
  Serial.println(F("startup: hold 45 deg, then repeating cycle"));
  write_all_us(nearUs);
  holdUs = nearUs;
  delay(400);
#endif
}
#endif

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
#if RR_BRINGUP_DEBUG
  Serial.print(F(" fault="));
  Serial.print(turnout[idx].fault_name());
#endif
  Serial.print(F(" rest_us="));
  Serial.print(restUs);
  Serial.print(F(" hold_us="));
  Serial.print(holdUs);
  Serial.print(F(" us="));
  Serial.println(turnout[idx].pulse_us());
}

static void help() {
#if RR_BRINGUP_DEBUG
  Serial.println(F("TurnoutBringup Mega (DEBUG on, rest 90 deg)"));
#else
  Serial.println(F("TurnoutBringup Mega (DEBUG off, cycle 45-135 then 90)"));
#endif
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

  nearUs = sg90_us_from_deg(kNearDeg);
  midUs = sg90_us_from_deg(kMidDeg);
  farUs = sg90_us_from_deg(kFarDeg);
  restUs = midUs;
  holdUs = restUs;

  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    const int pin = rr_limit_pin(i);
    if (pin >= 0) {
      pinMode(pin, INPUT);
    }
    turnout[i].set_config(turnout_default_sg90());
  }

#if RR_USE_KS0258
  startup_pwm();
#endif

  help();
#if RR_USE_KS0258 && !RR_BRINGUP_DEBUG
  demo_begin_phase(kDemoToFar);
#endif
}

void loop() {
  const uint32_t now = millis();
  unsigned i;
  handle_serial(now);
#if RR_USE_KS0258 && !RR_BRINGUP_DEBUG
  const bool driven = any_drive_enabled();
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    sample_one(i, now, driven);
  }
  if (!driven) {
    demo_tick(now);
  }
#else
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    sample_one(i, now, true);
  }
#endif
  if ((now - lastStatusMs) >= 1000u) {
    lastStatusMs = now;
    print_one(selected);
  }
}
