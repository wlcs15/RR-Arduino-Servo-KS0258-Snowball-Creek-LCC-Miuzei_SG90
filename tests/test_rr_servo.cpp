#include "LimitLadder.h"
#include "TurnoutChannel.h"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void expect_eq_int(const char *name, int got, int want) {
  if (got != want) {
    fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
    ++failures;
  }
}

static void test_ladder_bands() {
  LimitLadderConfig cfg = limit_ladder_default_10bit();
  expect_eq_int("neither", limit_ladder_decode(1023, cfg), LIMIT_NEITHER);
  expect_eq_int("neither-edge", limit_ladder_decode(901, cfg), LIMIT_NEITHER);
  expect_eq_int("thrown", limit_ladder_decode(330, cfg), LIMIT_THROWN);
  expect_eq_int("thrown-edge", limit_ladder_decode(251, cfg), LIMIT_THROWN);
  expect_eq_int("closed", limit_ladder_decode(185, cfg), LIMIT_CLOSED);
  expect_eq_int("closed-edge", limit_ladder_decode(151, cfg), LIMIT_CLOSED);
  expect_eq_int("both", limit_ladder_decode(125, cfg), LIMIT_BOTH);
  expect_eq_int("both-zero", limit_ladder_decode(0, cfg), LIMIT_BOTH);
}

static void test_ladder_scaled_12bit() {
  LimitLadderConfig cfg = limit_ladder_scaled(4095);
  expect_eq_int("12bit-neither", limit_ladder_decode(4095, cfg), LIMIT_NEITHER);
  // 330/1023*4095 ≈ 1320
  expect_eq_int("12bit-thrown", limit_ladder_decode(1320, cfg), LIMIT_THROWN);
}

static void test_filter_average() {
  LimitLadderFilter f(4);
  expect_eq_int("n1", f.push(100), 100);
  expect_eq_int("n2", f.push(200), 150);
  expect_eq_int("n3", f.push(300), 200);
  expect_eq_int("n4", f.push(400), 250);
  expect_eq_int("n5", f.push(500), 350);
}

static void test_turnout_arrives_on_limit() {
  TurnoutChannel t;
  t.command(TURNOUT_CMD_THROWN, 1000);
  expect_eq_int("moving", t.motion(), TURNOUT_MOVING_THROWN);
  expect_eq_int("drive-on", t.drive_enabled(), 1);
  expect_eq_int("pulse", t.pulse_us(), 2000);
  t.update(1100, LIMIT_NEITHER);
  expect_eq_int("still-moving", t.motion(), TURNOUT_MOVING_THROWN);
  t.update(1200, LIMIT_THROWN);
  expect_eq_int("idle-arrived", t.motion(), TURNOUT_IDLE);
  expect_eq_int("arrived", t.arrived(), 1);
  t.update(1500, LIMIT_THROWN);
  expect_eq_int("released", t.drive_enabled(), 0);
}

static void test_turnout_timeout_and_both() {
  TurnoutChannel t;
  t.command(TURNOUT_CMD_CLOSED, 0);
  t.update(4000, LIMIT_NEITHER);
  expect_eq_int("timeout-motion", t.motion(), TURNOUT_FAULT);
  expect_eq_int("timeout-fault", t.fault(), TURNOUT_FAULT_TIMEOUT);
  expect_eq_int("timeout-drive", t.drive_enabled(), 0);

  TurnoutChannel t2;
  t2.command(TURNOUT_CMD_THROWN, 0);
  t2.update(10, LIMIT_BOTH);
  expect_eq_int("both-fault", t2.fault(), TURNOUT_FAULT_BOTH_LIMITS);
  expect_eq_int("both-drive", t2.drive_enabled(), 0);
}

int main() {
  test_ladder_bands();
  test_ladder_scaled_12bit();
  test_filter_average();
  test_turnout_arrives_on_limit();
  test_turnout_timeout_and_both();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  puts("ok");
  return 0;
}
