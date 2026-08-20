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
// Events for 05.01.01.01.A5.02:
//   consume  ...:00 throw  ...:01 close
//   produce  ...:10 neither  ...:11 thrown  ...:12 closed  ...:13 both
// Do not send t/c or throw/close events until the ladder is wired.

#ifdef OPTIMIZE_MEMORY
#pragma GCC optimize ("Os")
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

#if RR_USE_KS0258
#include <Adafruit_PWMServoDriver.h>
static Adafruit_PWMServoDriver pwm(RR_PCA9685_ADDR);
#else
#include <Servo.h>
static Servo fallbackServo;
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
static const uint64_t kEvtThrow = kEventBase + 0x00ULL;
static const uint64_t kEvtClose = kEventBase + 0x01ULL;
static const uint64_t kEvtNeither = kEventBase + 0x10ULL;
static const uint64_t kEvtThrown = kEventBase + 0x11ULL;
static const uint64_t kEvtClosed = kEventBase + 0x12ULL;
static const uint64_t kEvtBoth = kEventBase + 0x13ULL;

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
static TurnoutChannel turnout;
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
  if (!fallbackServo.attached()) {
    fallbackServo.attach(rr_fallback_pwm_pin(0));
  }
  fallbackServo.writeMicroseconds(us);
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

static uint64_t event_for_limit(LimitState lim) {
  if (lim == LIMIT_THROWN) {
    return kEvtThrown;
  }
  if (lim == LIMIT_CLOSED) {
    return kEvtClosed;
  }
  if (lim == LIMIT_BOTH) {
    return kEvtBoth;
  }
  return kEvtNeither;
}

static void handle_command(TurnoutCommand cmd) {
  turnout.command(cmd, millis());
}

static void incoming_event(struct lcc_context *, uint64_t event_id) {
  if (event_id == kEvtThrow) {
    handle_command(TURNOUT_CMD_THROWN);
  } else if (event_id == kEvtClose) {
    handle_command(TURNOUT_CMD_CLOSED);
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
                            event_for_limit(lim));
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
    handle_command(TURNOUT_CMD_THROWN);
  } else if (key == 'c') {
    handle_command(TURNOUT_CMD_CLOSED);
  } else if (key == 's') {
    RR_LCC_PRINT(F("limit="));
    RR_LCC_PRINT(limit_state_name(turnout.last_limit()));
    RR_LCC_PRINT(F(" motion="));
    RR_LCC_PRINT(turnout.motion_name());
    RR_LCC_PRINT(F(" hold_us="));
    RR_LCC_PRINTLN(holdUs);
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
                                          "Mega+Snowball", "v1.00");
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
      "<softwareVersion>v1.00</softwareVersion>"
      "</identification>"
      "<acdi/>"
      "</cdi>";
  lcc_memory_set_cdi(mem, (void *)kCdi, (int)sizeof(kCdi),
                     LCC_MEMORY_CDI_FLAG_ARDUINO_PROGMEM);
  struct lcc_event_context *evt = lcc_event_new(ctx);
  lcc_event_set_incoming_event_function(evt, incoming_event);
  lcc_event_add_event_consumed(evt, kEvtThrow);
  lcc_event_add_event_consumed(evt, kEvtClose);
  lcc_event_add_event_produced(evt, kEvtNeither);
  lcc_event_add_event_produced(evt, kEvtThrown);
  lcc_event_add_event_produced(evt, kEvtClosed);
  lcc_event_add_event_produced(evt, kEvtBoth);
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

void setup() {
#ifndef LCC_ON
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
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
  turnout.set_config(cfg);

#if RR_USE_KS0258
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);
#endif

  setup_can();
  setup_lcc(read_unique_id());

  RR_LCC_PRINTLN(F("LccTurnoutNode LibLCC; one cycle then hold 90"));
  RR_LCC_PRINT(F("RR_USE_KS0258="));
  RR_LCC_PRINTLN(RR_USE_KS0258);
  RR_LCC_PRINTLN(F("Do not t/c until ladder is wired"));

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
  /* Ladder is unwired: analog floats ~thrown and must not "arrive" a move. */
  turnout.update(now, LIMIT_NEITHER);
  if (turnout.last_command() != TURNOUT_CMD_NONE) {
    warmupDone = true;
    write_hold_us(turnout.pulse_us());
  } else if (!warmupDone) {
    demo_tick(now);
  } else {
    write_hold_us(midUs);
  }
  maybe_announce_limit(lim);
  if ((now - lastStatusMs) >= 1000u) {
    lastStatusMs = now;
    RR_LCC_PRINT(F("hold_us="));
    RR_LCC_PRINT(holdUs);
    RR_LCC_PRINT(F(" motion="));
    RR_LCC_PRINTLN(turnout.motion_name());
  }
}
