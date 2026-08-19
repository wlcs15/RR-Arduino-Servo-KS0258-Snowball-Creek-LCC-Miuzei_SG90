#include "LimitLadder.h"
#include "TurnoutChannel.h"
#include "unity.h"

static void test_ladder_bands(void) {
  const LimitLadderConfig cfg = limit_ladder_default_10bit();
  TEST_ASSERT_EQUAL(LIMIT_NEITHER, limit_ladder_decode(1023, cfg));
  TEST_ASSERT_EQUAL(LIMIT_NEITHER, limit_ladder_decode(901, cfg));
  TEST_ASSERT_EQUAL(LIMIT_THROWN, limit_ladder_decode(330, cfg));
  TEST_ASSERT_EQUAL(LIMIT_THROWN, limit_ladder_decode(251, cfg));
  TEST_ASSERT_EQUAL(LIMIT_CLOSED, limit_ladder_decode(185, cfg));
  TEST_ASSERT_EQUAL(LIMIT_CLOSED, limit_ladder_decode(151, cfg));
  TEST_ASSERT_EQUAL(LIMIT_BOTH, limit_ladder_decode(125, cfg));
  TEST_ASSERT_EQUAL(LIMIT_BOTH, limit_ladder_decode(0, cfg));
}

static void test_ladder_scaled_12bit(void) {
  const LimitLadderConfig cfg = limit_ladder_scaled(4095);
  TEST_ASSERT_EQUAL(LIMIT_NEITHER, limit_ladder_decode(4095, cfg));
  TEST_ASSERT_EQUAL(LIMIT_THROWN, limit_ladder_decode(1320, cfg));
}

static void test_filter_average(void) {
  LimitLadderFilter filter(4);
  TEST_ASSERT_EQUAL(100, filter.push(100));
  TEST_ASSERT_EQUAL(150, filter.push(200));
  TEST_ASSERT_EQUAL(200, filter.push(300));
  TEST_ASSERT_EQUAL(250, filter.push(400));
  TEST_ASSERT_EQUAL(350, filter.push(500));
}

static void test_turnout_arrives_on_limit(void) {
  TurnoutChannel ch;
  ch.command(TURNOUT_CMD_THROWN, 1000);
  TEST_ASSERT_EQUAL(TURNOUT_MOVING_THROWN, ch.motion());
  TEST_ASSERT_EQUAL(1, ch.drive_enabled() ? 1 : 0);
  TEST_ASSERT_EQUAL(2000, ch.pulse_us());
  ch.update(1100, LIMIT_NEITHER);
  TEST_ASSERT_EQUAL(TURNOUT_MOVING_THROWN, ch.motion());
  ch.update(1200, LIMIT_THROWN);
  TEST_ASSERT_EQUAL(TURNOUT_IDLE, ch.motion());
  TEST_ASSERT_EQUAL(1, ch.arrived() ? 1 : 0);
  ch.update(1500, LIMIT_THROWN);
  TEST_ASSERT_EQUAL(0, ch.drive_enabled() ? 1 : 0);
}

static void test_turnout_timeout_and_both(void) {
  TurnoutChannel ch;
  ch.command(TURNOUT_CMD_CLOSED, 0);
  ch.update(4000, LIMIT_NEITHER);
  TEST_ASSERT_EQUAL(TURNOUT_FAULT, ch.motion());
  TEST_ASSERT_EQUAL(TURNOUT_FAULT_TIMEOUT, ch.fault());
  TEST_ASSERT_EQUAL(0, ch.drive_enabled() ? 1 : 0);

  TurnoutChannel ch2;
  ch2.command(TURNOUT_CMD_THROWN, 0);
  ch2.update(10, LIMIT_BOTH);
  TEST_ASSERT_EQUAL(TURNOUT_FAULT_BOTH_LIMITS, ch2.fault());
  TEST_ASSERT_EQUAL(0, ch2.drive_enabled() ? 1 : 0);
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ladder_bands);
  RUN_TEST(test_ladder_scaled_12bit);
  RUN_TEST(test_filter_average);
  RUN_TEST(test_turnout_arrives_on_limit);
  RUN_TEST(test_turnout_timeout_and_both);
  return UNITY_END();
}
