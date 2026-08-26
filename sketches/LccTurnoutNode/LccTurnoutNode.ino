// Mega 2560 + Snowball Creek LCC shield + KS0258 (or Mega D44 if
// RR_USE_KS0258=0).
//
// Snowball Creek user guide SCE-230903R2A: LibLCC via Library Manager
// with ACAN2515 + M95_EEPROM. Do not cut SB1/SB2/SB3; D3 (DCC) and
// D5/D6 (shield LEDs) are left unused.
//
// Default: UART debug ON, one 45-135-45-90 startup cycle, then hold 90
// waiting for LCC. LibLCC ON.
//   -DLCC_ON           silence UART (later memory pass)
//   -DOPTIMIZE_MEMORY  -Os / smaller CAN buffers (later memory pass)
//
// Application sources stay BSD-2-Clause. Linking LibLCC (GPL-2.0)
// makes the firmware image GPL-2.0. Do not copy LibLCC into lib/.
//
// Events on node 05.01.01.01.A5.02 (channel in byte 6, 8-byte event):
//   ch0 throw/close  05.01.01.01.A5.02.00.00 / ...A5.02.00.01
//   ch1 throw/close  05.01.01.01.A5.02.01.00 / ...A5.02.01.01
//   ch0 limits       ...A5.02.00.10 .. .13  (neither/thrown/closed/both)
// OwlThree map: .A5.01 display Wi-Fi, .A5.02 this Mega CAN, .A5.03 servo
// Wi-Fi, .A5.04 S3 panel. Next free .A5.05.
// Do not send throw/close until the ladder is wired (pulses still 0/180).

#ifdef OPTIMIZE_MEMORY
#pragma GCC optimize ("Os")
#endif

#ifndef LIBLCC_EVENT_LIST_STATIC_SIZE
#define LIBLCC_EVENT_LIST_STATIC_SIZE 16
#endif

#include <SPI.h>
#include <Wire.h>
#include <string.h>

#ifndef RR_CAN_CHIP
#define RR_CAN_CHIP 2518
#endif
#if RR_CAN_CHIP == 2518
#include <ACAN2517.h>
#else
#include <ACAN2515.h>
#endif
#include <M95_EEPROM.h>
#include <lcc.h>
#include <lcc-event.h>
#include <lcc-datagram.h>
#include <lcc-memory.h>

#include <BoardPins.h>
#include <LimitLadder.h>
#include <TurnoutChannel.h>
#if defined(__has_include)
#if __has_include("git_version.inc")
#include "git_version.inc"
#endif
#endif
#include <GitVersion.h>

#if RR_USE_KS0258
#include <Adafruit_PWMServoDriver.h>
static Adafruit_PWMServoDriver pwm(RR_PCA9685_ADDR);
#else
#include <Servo.h>
static Servo fallbackServo[2];
#endif

#ifdef LCC_ON
#define RR_LCC_PRINT(...) ((void)0)
#define RR_LCC_PRINTLN(...) ((void)0)
#else
#define RR_LCC_PRINT(...) Serial.print(__VA_ARGS__)
#define RR_LCC_PRINTLN(...) Serial.println(__VA_ARGS__)
#endif

static const unsigned kNearDeg = 45u;
static const unsigned kMidDeg = 90u;
static const unsigned kFarDeg = 135u;
static const uint32_t kSweepMs = 3000u;
static const uint32_t kHalfSweepMs = 1500u;
static const uint32_t kHoldMidMs = 400u;
static const uint32_t kXtalsHz = 16UL * 1000UL * 1000UL;
static const uint32_t kCanBps = 125UL * 1000UL;
// OwlThree 05.01.01.01.A5.*  (.A5.01 = D1 R32 display; this node = .A5.02)
static const uint64_t kOwlThreeNodeId = 0x05010101A502ULL;
static const uint64_t kEventBase = 0x05010101A5020000ULL;
static const unsigned kLiveServos = 2u;

static uint64_t evt_throw(unsigned ch) {
  return kEventBase + ((uint64_t)ch << 8);
}

static uint64_t evt_close(unsigned ch) {
  return kEventBase + ((uint64_t)ch << 8) + 1ULL;
}

static uint64_t evt_limit(unsigned ch, unsigned code) {
  return kEventBase + ((uint64_t)ch << 8) + 0x10ULL + (uint64_t)code;
}

enum {
  kDemoToFar = 0,
  kDemoToNear,
  kDemoToMid,
  kDemoHoldMid
};

struct IdPage {
  uint64_t node_id;
  uint16_t id_version;
  char manufacturer[32];
  char part_number[21];
  char hw_version[12];
};

#if RR_CAN_CHIP == 2518
static ACAN2517 can(RR_LCC_MCP2515_CS, SPI, RR_LCC_MCP2515_IRQ);
#else
static ACAN2515 can(RR_LCC_MCP2515_CS, SPI, RR_LCC_MCP2515_IRQ);
#endif
static M95_EEPROM eeprom(SPI, RR_LCC_EEPROM_CS, 256, 3, true);
static lcc_context *ctx;
static CANMessage frame;
static struct lcc_can_frame lccFrame;
static uint32_t claimAliasAtMs = 0;
static uint32_t lastStatusMs = 0;
static LimitLadderConfig ladderCfg;
static LimitLadderFilter ladderFilter(4);
static TurnoutChannel turnout[kLiveServos];
static LimitState lastAnnounced = LIMIT_NEITHER;
static bool announcedInit = false;
static uint16_t nearUs = (uint16_t)RR_SG90_US_0;
static uint16_t midUs = (uint16_t)RR_SG90_US_90;
static uint16_t farUs = (uint16_t)RR_SG90_US_180;
static uint16_t holdUs = (uint16_t)RR_SG90_US_90;
static uint8_t demoPhase = kDemoToFar;
static uint32_t demoT0 = 0;
static uint16_t demoFromUs = (uint16_t)RR_SG90_US_90;
static uint16_t demoToUs = (uint16_t)RR_SG90_US_90;
static uint32_t demoDurMs = kSweepMs;
static bool warmupDone = false;

static uint16_t sg90_us_from_deg(unsigned deg) {
  if (deg > 180u) {
    deg = 180u;
  }
  return (uint16_t)((uint32_t)RR_SG90_US_0 +
                    ((uint32_t)(RR_SG90_US_180 - RR_SG90_US_0) * (uint32_t)deg) /
                        180u);
}

static void write_hold_us(uint16_t us) {
  holdUs = us;
#if RR_USE_KS0258
  unsigned i;
  for (i = 0; i < (unsigned)RR_TURNOUT_COUNT; ++i) {
    pwm.writeMicroseconds((uint8_t)i, us);
  }
#else
  unsigned i;
  for (i = 0; i < kLiveServos; ++i) {
    if (!fallbackServo[i].attached()) {
      fallbackServo[i].attach(rr_fallback_pwm_pin(i));
    }
    fallbackServo[i].writeMicroseconds(us);
  }
#endif
}

static void write_ch_us(unsigned ch, uint16_t us) {
  if (ch >= kLiveServos) {
    return;
  }
#if RR_USE_KS0258
  pwm.writeMicroseconds((uint8_t)ch, us);
#else
  if (!fallbackServo[ch].attached()) {
    fallbackServo[ch].attach(rr_fallback_pwm_pin(ch));
  }
  fallbackServo[ch].writeMicroseconds(us);
#endif
}

static uint16_t lerp_us(uint16_t fromUs, uint16_t toUs, uint32_t elapsed,
                        uint32_t duration) {
  if (elapsed >= duration) {
    return toUs;
  }
  const int32_t delta = (int32_t)toUs - (int32_t)fromUs;
  return (uint16_t)((int32_t)fromUs + (delta * (int32_t)elapsed) / (int32_t)duration);
}

static void demo_begin_phase(uint8_t phase) {
  demoPhase = phase;
  demoT0 = millis();
  if (phase == kDemoToFar) {
    demoFromUs = nearUs;
    demoToUs = farUs;
    demoDurMs = kSweepMs;
    RR_LCC_PRINTLN(F("cycle: 45 -> 135 in 3 s"));
  } else if (phase == kDemoToNear) {
    demoFromUs = farUs;
    demoToUs = nearUs;
    demoDurMs = kSweepMs;
    RR_LCC_PRINTLN(F("cycle: 135 -> 45 in 3 s"));
  } else if (phase == kDemoToMid) {
    demoFromUs = nearUs;
    demoToUs = midUs;
    demoDurMs = kHalfSweepMs;
    RR_LCC_PRINTLN(F("cycle: 45 -> 90 in 1.5 s"));
  } else {
    demoFromUs = midUs;
    demoToUs = midUs;
    demoDurMs = kHoldMidMs;
    RR_LCC_PRINTLN(F("hold 90 deg, waiting for LCC"));
    write_hold_us(midUs);
    warmupDone = true;
  }
}

static void demo_next_phase(void) {
  if (demoPhase >= kDemoHoldMid) {
    return;
  }
  demo_begin_phase((uint8_t)(demoPhase + 1u));
}

static void demo_tick(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - demoT0;
  if (demoPhase == kDemoHoldMid) {
    write_hold_us(midUs);
    return;
  }
  write_hold_us(lerp_us(demoFromUs, demoToUs, elapsed, demoDurMs));
  if (elapsed >= demoDurMs) {
    write_hold_us(demoToUs);
    demo_next_phase();
  }
}

static int lcc_write(struct lcc_context *, struct lcc_can_frame *out) {
  frame.id = out->can_id;
  frame.len = out->can_len;
  frame.rtr = false;
  frame.ext = true;
  memcpy(frame.data, out->data, 8);
  if (can.tryToSend(frame)) {
    return LCC_OK;
  }
  return LCC_ERROR_TX;
}

static int lcc_buffer_size(struct lcc_context *) {
#if RR_CAN_CHIP == 2518
  return (int)(can.driverTransmitBufferSize() - can.driverTransmitBufferCount());
#else
  return can.transmitBufferSize(0) - can.transmitBufferCount(0);
#endif
}

static uint64_t event_for_limit(unsigned ch, LimitState lim) {
  if (lim == LIMIT_THROWN) {
    return evt_limit(ch, 1u);
  }
  if (lim == LIMIT_CLOSED) {
    return evt_limit(ch, 2u);
  }
  if (lim == LIMIT_BOTH) {
    return evt_limit(ch, 3u);
  }
  return evt_limit(ch, 0u);
}

static void incoming_event(struct lcc_context *, uint64_t event_id) {
  unsigned ch;
  for (ch = 0; ch < kLiveServos; ++ch) {
    if (event_id == evt_throw(ch)) {
      turnout[ch].command(TURNOUT_CMD_THROWN, millis());
      return;
    }
    if (event_id == evt_close(ch)) {
      turnout[ch].command(TURNOUT_CMD_CLOSED, millis());
      return;
    }
  }
}

static void maybe_announce_limit(LimitState lim) {
  if (announcedInit && lim == lastAnnounced) {
    return;
  }
  lastAnnounced = lim;
  announcedInit = true;
  if (ctx != NULL && lcc_context_current_state(ctx) != LCC_STATE_INHIBITED) {
    lcc_event_produce_event(lcc_context_get_event_context(ctx),
                            event_for_limit(0u, lim));
  }
  RR_LCC_PRINT(F("limit-event "));
  RR_LCC_PRINTLN(limit_state_name(lim));
}

static char cmd_key(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static void handle_serial() {
  if (!Serial.available()) {
    return;
  }
  const char key = cmd_key((char)Serial.read());
  if (key == 't') {
    turnout[0].command(TURNOUT_CMD_THROWN, millis());
  } else if (key == 'c') {
    turnout[0].command(TURNOUT_CMD_CLOSED, millis());
  } else if (key == 's') {
    RR_LCC_PRINT(F("ch0="));
    RR_LCC_PRINT(turnout[0].motion_name());
    RR_LCC_PRINT(F(" ch1="));
    RR_LCC_PRINTLN(turnout[1].motion_name());
  }
}

static void poll_can() {
  if (can.available()) {
    can.receive(frame);
    lccFrame.can_id = frame.id;
    lccFrame.can_len = frame.len;
    memcpy(lccFrame.data, frame.data, 8);
    lcc_context_incoming_frame(ctx, &lccFrame);
  }
}

static void claim_alias_if_ready() {
  if (millis() < claimAliasAtMs) {
    return;
  }
  if (lcc_context_current_state(ctx) != LCC_STATE_INHIBITED) {
    return;
  }
  if (lcc_context_claim_alias(ctx) != LCC_OK) {
    lcc_context_generate_alias(ctx);
    claimAliasAtMs = millis() + 220u;
    return;
  }
  RR_LCC_PRINT(F("alias=0x"));
  RR_LCC_PRINTLN(lcc_context_alias(ctx), HEX);
}

static uint64_t read_unique_id() {
  IdPage id;
  memset(&id, 0, sizeof(id));
  eeprom.read_id_page(sizeof(id), &id);
#ifndef LCC_ON
  char dotted[24];
  lcc_node_id_to_dotted_format(id.node_id, dotted, sizeof(dotted));
  RR_LCC_PRINT(F("factory eeprom="));
  RR_LCC_PRINTLN(dotted);
  lcc_node_id_to_dotted_format(kOwlThreeNodeId, dotted, sizeof(dotted));
  RR_LCC_PRINT(F("using node="));
  RR_LCC_PRINTLN(dotted);
#endif
  return kOwlThreeNodeId;
}

static void setup_lcc(uint64_t unique_id) {
  ctx = lcc_context_new();
  lcc_context_set_unique_identifer(ctx, unique_id);
  lcc_context_set_write_function(ctx, lcc_write, lcc_buffer_size);
  lcc_context_set_simple_node_information(ctx, "OwlThree", "RR-Servo-KS0258",
                                          "Mega+Snowball",
                                          RR_GIT_VERSION_LITERAL);
  lcc_context_set_simple_node_name_description(ctx, "RR-Servo-KS0258",
                                               "OwlThree turnout Mega");
  lcc_datagram_context_new(ctx);
  struct lcc_memory_context *mem = lcc_memory_new(ctx);
  static const char kCdi[] PROGMEM =
      "<?xml version='1.0'?>"
      "<cdi xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' "
      "xsi:noNamespaceSchemaLocation='http://openlcb.org/schema/cdi/1/1/cdi.xsd'>"
      "<identification>"
      "<manufacturer>OwlThree</manufacturer>"
      "<model>RR-Servo-KS0258</model>"
      "<hardwareVersion>Mega+Snowball</hardwareVersion>"
      "<softwareVersion>" RR_GIT_VERSION_LITERAL "</softwareVersion>"
      "</identification>"
      "<acdi/>"
      "</cdi>";
  lcc_memory_set_cdi(mem, (void *)kCdi, (int)sizeof(kCdi),
                     LCC_MEMORY_CDI_FLAG_ARDUINO_PROGMEM);
  struct lcc_event_context *evt = lcc_event_new(ctx);
  lcc_event_set_incoming_event_function(evt, incoming_event);
  {
    unsigned ch;
    for (ch = 0; ch < kLiveServos; ++ch) {
      lcc_event_add_event_consumed(evt, evt_throw(ch));
      lcc_event_add_event_consumed(evt, evt_close(ch));
    }
  }
  lcc_event_add_event_produced(evt, event_for_limit(0u, LIMIT_NEITHER));
  lcc_event_add_event_produced(evt, event_for_limit(0u, LIMIT_THROWN));
  lcc_event_add_event_produced(evt, event_for_limit(0u, LIMIT_CLOSED));
  lcc_event_add_event_produced(evt, event_for_limit(0u, LIMIT_BOTH));
  if (lcc_context_generate_alias(ctx) != LCC_OK) {
    RR_LCC_PRINTLN(F("alias generate fail"));
    while (1) {
    }
  }
  claimAliasAtMs = millis() + 220u;
}

static void setup_can() {
  uint32_t err = 0xFFFFFFFFu;
#if RR_CAN_CHIP == 2518
  RR_LCC_PRINTLN(F("CAN chip MCP2518"));
  ACAN2517Settings settings(ACAN2517Settings::OSC_40MHz_DIVIDED_BY_2, kCanBps);
#ifdef OPTIMIZE_MEMORY
  settings.mDriverReceiveFIFOSize = 4;
  settings.mDriverTransmitFIFOSize = 8;
#endif
  err = can.begin(settings, [] { can.isr(); });
  RR_LCC_PRINT(F("ACAN2517 begin=0x"));
  RR_LCC_PRINTLN(err, HEX);
#else
  RR_LCC_PRINTLN(F("CAN chip MCP2515"));
  ACAN2515Settings settings(kXtalsHz, kCanBps);
  settings.mRequestedMode = ACAN2515Settings::NormalMode;
#ifdef OPTIMIZE_MEMORY
  settings.mReceiveBufferSize = 4;
  settings.mTransmitBuffer0Size = 8;
#elif defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_MEGA)
  settings.mReceiveBufferSize = 16;
  settings.mTransmitBuffer0Size = 16;
#else
  settings.mReceiveBufferSize = 4;
  settings.mTransmitBuffer0Size = 8;
#endif
  settings.mPropagationSegment = 1;
  settings.mTripleSampling = false;
  settings.mPhaseSegment1 = 3;
  settings.mPhaseSegment2 = 3;
  settings.mSJW = 1;
  settings.mBitRatePrescaler = 8;
  err = can.begin(settings, [] { can.isr(); });
  RR_LCC_PRINT(F("ACAN2515 begin=0x"));
  RR_LCC_PRINTLN(err, HEX);
  if (err == ACAN2515::kNoMCP2515) {
    RR_LCC_PRINTLN(F("no MCP2515 (shield may be MCP2518 Rev 4)"));
  }
#endif
  if (err != 0) {
    RR_LCC_PRINTLN(F("CAN init fail; servo cycle still runs"));
  }
}

static void print_reset_reason(void) {
#ifndef LCC_ON
  uint8_t r = MCUSR;
  MCUSR = 0;
  RR_LCC_PRINT(F("reset 0x"));
  RR_LCC_PRINT(r, HEX);
  if (r & _BV(PORF)) {
    RR_LCC_PRINT(F(" poweron"));
  }
  if (r & _BV(EXTRF)) {
    RR_LCC_PRINT(F(" extrst"));
  }
  if (r & _BV(BORF)) {
    RR_LCC_PRINT(F(" brownout"));
  }
  if (r & _BV(WDRF)) {
    RR_LCC_PRINT(F(" wdt"));
  }
  RR_LCC_PRINTLN();
#endif
}

#ifndef LCC_ON
extern "C" void __assert(const char *func, const char *file, int line,
                         const char *expr) {
  RR_LCC_PRINT(F("assert "));
  RR_LCC_PRINT(file);
  RR_LCC_PRINT(':');
  RR_LCC_PRINT(line);
  RR_LCC_PRINT(' ');
  RR_LCC_PRINTLN(expr);
  (void)func;
  for (;;) {
  }
}
#endif

void setup() {
#ifndef LCC_ON
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  print_reset_reason();
  RR_LCC_PRINT(F("firmware "));
  RR_LCC_PRINTLN(F(RR_GIT_VERSION_LITERAL));
#endif
  delay(2000);

  nearUs = sg90_us_from_deg(kNearDeg);
  midUs = sg90_us_from_deg(kMidDeg);
  farUs = sg90_us_from_deg(kFarDeg);
  holdUs = nearUs;

  pinMode(RR_LIMIT_PIN_0, INPUT);
  pinMode(RR_LCC_EEPROM_CS, OUTPUT);
  digitalWrite(RR_LCC_EEPROM_CS, HIGH);

  SPI.begin();
  eeprom.begin();
  ladderCfg = limit_ladder_default_10bit();
  TurnoutConfig cfg = turnout_default_sg90();
  cfg.releasePwmWhenIdle = false;
  {
    unsigned ch;
    for (ch = 0; ch < kLiveServos; ++ch) {
      turnout[ch].set_config(cfg);
    }
  }

#if RR_USE_KS0258
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
#endif

  setup_can();
  setup_lcc(read_unique_id());

  RR_LCC_PRINTLN(F("LccTurnoutNode ch0+ch1; hold 90 then one cycle"));
  RR_LCC_PRINTLN(F("Do not t/c until ladder is wired"));

  write_hold_us(midUs);
  delay(1000);
  write_hold_us(nearUs);
  delay(400);
  demo_begin_phase(kDemoToFar);
}

void loop() {
  const uint32_t now = millis();
  poll_can();
  claim_alias_if_ready();
#ifndef LCC_ON
  handle_serial();
#endif
  const int avg = ladderFilter.push(analogRead(RR_LIMIT_PIN_0));
  const LimitState lim = limit_ladder_decode(avg, ladderCfg);
  {
    unsigned ch;
    bool anyCmd = false;
    for (ch = 0; ch < kLiveServos; ++ch) {
      turnout[ch].update(now, LIMIT_NEITHER);
      if (turnout[ch].last_command() != TURNOUT_CMD_NONE) {
        anyCmd = true;
      }
    }
    if (anyCmd) {
      warmupDone = true;
    }
    if (!warmupDone) {
      demo_tick(now);
    } else {
      for (ch = 0; ch < kLiveServos; ++ch) {
        const uint16_t us =
            (turnout[ch].last_command() != TURNOUT_CMD_NONE)
                ? turnout[ch].pulse_us()
                : midUs;
        write_ch_us(ch, us);
      }
    }
  }
  maybe_announce_limit(lim);
  if ((now - lastStatusMs) >= 1000u) {
    lastStatusMs = now;
    RR_LCC_PRINT(F("ch0_us="));
    RR_LCC_PRINT(turnout[0].pulse_us());
    RR_LCC_PRINT(F(" ch1_us="));
    RR_LCC_PRINTLN(turnout[1].pulse_us());
  }
}
