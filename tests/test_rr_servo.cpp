#include "LimitLadder.h"
#include "TurnoutChannel.h"
#include "WifiStaBind.h"
#include "WifiHubPick.h"
#include "GitVersion.h"
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
  TEST_ASSERT_EQUAL_STRING("moving-thrown", ch.motion_name());
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
  TEST_ASSERT_EQUAL_STRING("timeout", ch.fault_name());

  TurnoutChannel ch2;
  ch2.command(TURNOUT_CMD_THROWN, 0);
  ch2.update(10, LIMIT_BOTH);
  TEST_ASSERT_EQUAL(TURNOUT_FAULT_BOTH_LIMITS, ch2.fault());
  TEST_ASSERT_EQUAL(0, ch2.drive_enabled() ? 1 : 0);
  TEST_ASSERT_EQUAL_STRING("both-limits", ch2.fault_name());
  TEST_ASSERT_EQUAL_STRING("fault", ch2.motion_name());
}

static void test_limit_state_names(void) {
  TEST_ASSERT_EQUAL_STRING("neither", limit_state_name(LIMIT_NEITHER));
  TEST_ASSERT_EQUAL_STRING("thrown", limit_state_name(LIMIT_THROWN));
  TEST_ASSERT_EQUAL_STRING("closed", limit_state_name(LIMIT_CLOSED));
  TEST_ASSERT_EQUAL_STRING("both", limit_state_name(LIMIT_BOTH));
  TEST_ASSERT_EQUAL_STRING("?", limit_state_name((LimitState)99));
}

static void test_filter_window_and_reset(void) {
  LimitLadderFilter tiny(0);
  TEST_ASSERT_EQUAL(10, tiny.push(10));
  LimitLadderFilter huge(20);
  TEST_ASSERT_EQUAL(7, huge.push(7));
  LimitLadderFilter f(2);
  TEST_ASSERT_EQUAL(10, f.push(10));
  TEST_ASSERT_EQUAL(15, f.push(20));
  f.reset();
  TEST_ASSERT_EQUAL(5, f.push(5));
}

static void test_turnout_names_config_and_none(void) {
  TurnoutChannel ch;
  TEST_ASSERT_EQUAL_STRING("idle", ch.motion_name());
  TEST_ASSERT_EQUAL_STRING("none", ch.fault_name());
  ch.command(TURNOUT_CMD_NONE, 0);
  TEST_ASSERT_EQUAL(TURNOUT_IDLE, ch.motion());
  TurnoutConfig cfg = turnout_default_sg90();
  cfg.releasePwmWhenIdle = false;
  ch.set_config(cfg);
  TEST_ASSERT_EQUAL(cfg.closedPulseUs, ch.config().closedPulseUs);
  ch.command(TURNOUT_CMD_CLOSED, 100);
  TEST_ASSERT_EQUAL_STRING("moving-closed", ch.motion_name());
  TEST_ASSERT_EQUAL(TURNOUT_CMD_CLOSED, ch.last_command());
  ch.update(110, LIMIT_NEITHER);
  TEST_ASSERT_EQUAL(LIMIT_NEITHER, ch.last_limit());
  TEST_ASSERT_EQUAL_STRING("moving-closed", ch.motion_name());
  ch.update(120, LIMIT_CLOSED);
  TEST_ASSERT_EQUAL(1, ch.arrived() ? 1 : 0);
  ch.update(500, LIMIT_CLOSED);
  TEST_ASSERT_EQUAL(1, ch.drive_enabled() ? 1 : 0);
}

static void test_wifi_sta_bind_empty_psk_not_wpa2_ready(void) {
  char live_ssid[33] = "SRIF2333";
  char live_psk[65];
  WifiStaBind bind;
  live_psk[0] = '\0';
  wifi_sta_bind_copy(&bind, live_ssid, live_psk);
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_PSK, wifi_sta_bind_status(&bind));
  TEST_ASSERT_EQUAL(0, wifi_sta_bind_wpa2_ready(&bind));
}

static void test_wifi_sta_bind_copy_ignores_later_unwrap(void) {
  char live_ssid[33] = "SRIF2333";
  char live_psk[65];
  WifiStaBind bind;
  live_psk[0] = '\0';
  wifi_sta_bind_copy(&bind, live_ssid, live_psk);
  strncpy(live_psk, "unit-test-psk-18ch", sizeof(live_psk) - 1);
  live_psk[sizeof(live_psk) - 1] = '\0';
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_PSK, wifi_sta_bind_status(&bind));
  TEST_ASSERT_EQUAL(0, wifi_sta_bind_wpa2_ready(&bind));
  TEST_ASSERT_EQUAL('\0', bind.psk[0]);
  wifi_sta_bind_copy(&bind, live_ssid, live_psk);
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_OK, wifi_sta_bind_status(&bind));
  TEST_ASSERT_EQUAL(1, wifi_sta_bind_wpa2_ready(&bind));
  TEST_ASSERT_EQUAL_STRING("unit-test-psk-18ch", bind.psk);
  wifi_sta_bind_clear(&bind);
  TEST_ASSERT_EQUAL('\0', bind.psk[0]);
}

static void test_wifi_hub_skips_loopback_linklocal_self(void) {
  const uint32_t sta = wifi_hub_ipv4_octets(192, 168, 1, 233);
  const uint32_t mask = wifi_hub_ipv4_octets(255, 255, 255, 0);
  TEST_ASSERT_EQUAL(WIFI_HUB_RANK_SKIP,
                    wifi_hub_ipv4_rank(sta, mask, wifi_hub_ipv4_octets(127, 0, 0, 1)));
  TEST_ASSERT_EQUAL(WIFI_HUB_RANK_SKIP,
                    wifi_hub_ipv4_rank(sta, mask, wifi_hub_ipv4_octets(169, 254, 1, 1)));
  TEST_ASSERT_EQUAL(WIFI_HUB_RANK_SKIP,
                    wifi_hub_ipv4_rank(sta, mask, wifi_hub_ipv4_octets(224, 0, 0, 251)));
  TEST_ASSERT_EQUAL(WIFI_HUB_RANK_SKIP, wifi_hub_ipv4_rank(sta, mask, sta));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_usable(0));
}

static void test_wifi_hub_prefers_same_subnet_keeps_mdns_order(void) {
  const uint32_t sta = wifi_hub_ipv4_octets(192, 168, 1, 233);
  const uint32_t mask = wifi_hub_ipv4_octets(255, 255, 255, 0);
  uint32_t cands[5];
  int order[5];
  int n;
  cands[0] = wifi_hub_ipv4_octets(127, 0, 0, 1);
  cands[1] = wifi_hub_ipv4_octets(192, 168, 1, 82);
  cands[2] = wifi_hub_ipv4_octets(10, 0, 0, 1);
  cands[3] = wifi_hub_ipv4_octets(192, 168, 1, 57);
  cands[4] = sta;
  n = wifi_hub_order_indices(sta, mask, cands, 5, order, 5);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(1, order[0]);
  TEST_ASSERT_EQUAL(3, order[1]);
  TEST_ASSERT_EQUAL(2, order[2]);
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_rank(sta, mask, cands[1]));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_rank(sta, mask, cands[3]));
  TEST_ASSERT_EQUAL(1, wifi_hub_ipv4_rank(sta, mask, cands[2]));
}

static void test_wifi_hub_keeps_both_dual_home_ipv4s(void) {
  const uint32_t sta = wifi_hub_ipv4_octets(192, 168, 1, 233);
  const uint32_t mask = wifi_hub_ipv4_octets(255, 255, 255, 0);
  uint32_t cands[2];
  int order[2];
  int n;
  cands[0] = wifi_hub_ipv4_octets(192, 168, 1, 57);
  cands[1] = wifi_hub_ipv4_octets(192, 168, 1, 82);
  n = wifi_hub_order_indices(sta, mask, cands, 2, order, 2);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL(0, order[0]);
  TEST_ASSERT_EQUAL(1, order[1]);
  cands[0] = wifi_hub_ipv4_octets(192, 168, 1, 82);
  cands[1] = wifi_hub_ipv4_octets(192, 168, 1, 57);
  n = wifi_hub_order_indices(sta, mask, cands, 2, order, 2);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL(0, order[0]);
  TEST_ASSERT_EQUAL(1, order[1]);
}

static void test_wifi_hub_parse_rejects_garbage(void) {
  uint32_t ip = 99;
  uint32_t cands[2];
  int order[2];
  const uint32_t sta = wifi_hub_ipv4_octets(192, 168, 1, 233);
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse(NULL, &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("192.168.1.57", NULL));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("not-an-ip", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("999.1.1.1", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("1.999.1.1", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("1.1.999.1", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("1.1.1.999", &ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_ipv4_parse("127.0.0.1", &ip));
  TEST_ASSERT_EQUAL(1, wifi_hub_ipv4_parse("192.168.1.57", &ip));
  TEST_ASSERT_EQUAL(wifi_hub_ipv4_octets(192, 168, 1, 57), ip);
  TEST_ASSERT_EQUAL(1, wifi_hub_ipv4_usable(wifi_hub_ipv4_octets(169, 253, 0, 1)));
  TEST_ASSERT_EQUAL(1, wifi_hub_ipv4_rank(sta, 0, ip));
  TEST_ASSERT_EQUAL(0, wifi_hub_order_indices(ip, 0, NULL, 0, NULL, 0));
  TEST_ASSERT_EQUAL(0, wifi_hub_order_indices(sta, 0, cands, 2, NULL, 2));
  TEST_ASSERT_EQUAL(0, wifi_hub_order_indices(sta, 0, cands, 2, order, 0));
  TEST_ASSERT_EQUAL(0, wifi_hub_order_indices(sta, 0, cands, -1, order, 2));
  cands[0] = wifi_hub_ipv4_octets(192, 168, 1, 82);
  cands[1] = wifi_hub_ipv4_octets(192, 168, 1, 57);
  TEST_ASSERT_EQUAL(1, wifi_hub_order_indices(sta, wifi_hub_ipv4_octets(255, 255, 255, 0),
                                             cands, 2, order, 1));
  TEST_ASSERT_EQUAL(0, order[0]);
}

static void test_git_version_fallback_default(void) {
  TEST_ASSERT_EQUAL_STRING("v0.01+", RR_GIT_VERSION_STR(RR_GIT_VERSION));
}

static void test_wifi_sta_bind_null_and_empty_ssid(void) {
  WifiStaBind bind;
  wifi_sta_bind_clear(NULL);
  wifi_sta_bind_copy(NULL, "SRIF2333", "unit-test-psk-18ch");
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_SSID, wifi_sta_bind_status(NULL));
  TEST_ASSERT_EQUAL(0, wifi_sta_bind_wpa2_ready(NULL));
  wifi_sta_bind_copy(&bind, NULL, "unit-test-psk-18ch");
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_SSID, wifi_sta_bind_status(&bind));
  wifi_sta_bind_copy(&bind, "", "unit-test-psk-18ch");
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_SSID, wifi_sta_bind_status(&bind));
  wifi_sta_bind_copy(&bind, "SRIF2333", NULL);
  TEST_ASSERT_EQUAL(WIFI_STA_BIND_NO_PSK, wifi_sta_bind_status(&bind));
}

void setUp(void) {}
void tearDown(void) {}

int rr_servo_run_unity_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ladder_bands);
  RUN_TEST(test_ladder_scaled_12bit);
  RUN_TEST(test_filter_average);
  RUN_TEST(test_turnout_arrives_on_limit);
  RUN_TEST(test_turnout_timeout_and_both);
  RUN_TEST(test_limit_state_names);
  RUN_TEST(test_filter_window_and_reset);
  RUN_TEST(test_turnout_names_config_and_none);
  RUN_TEST(test_git_version_fallback_default);
  RUN_TEST(test_wifi_sta_bind_empty_psk_not_wpa2_ready);
  RUN_TEST(test_wifi_sta_bind_copy_ignores_later_unwrap);
  RUN_TEST(test_wifi_sta_bind_null_and_empty_ssid);
  RUN_TEST(test_wifi_hub_skips_loopback_linklocal_self);
  RUN_TEST(test_wifi_hub_prefers_same_subnet_keeps_mdns_order);
  RUN_TEST(test_wifi_hub_keeps_both_dual_home_ipv4s);
  RUN_TEST(test_wifi_hub_parse_rejects_garbage);
  return UNITY_END();
}

#ifndef ARDUINO
int main(void) { return rr_servo_run_unity_tests(); }
#endif

