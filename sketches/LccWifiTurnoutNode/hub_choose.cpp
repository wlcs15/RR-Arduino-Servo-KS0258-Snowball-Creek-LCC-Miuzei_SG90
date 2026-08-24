// ESP32-only: order dual-homed JMRI IPv4 ads. No Ubuntu net changes.
#if defined(ARDUINO_ARCH_ESP32)

#include "WifiHubPick.h"

#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void hub_pick_log_count(int n) {
  Serial.print(F("[HubPick] "));
  Serial.print(n);
  Serial.println(F(" IPv4 candidate(s)"));
}

static void hub_pick_log_ip(uint32_t ip_nbo) {
  char dotted[16];
  const uint32_t hip = ntohl(ip_nbo);
  snprintf(dotted, sizeof(dotted), "%u.%u.%u.%u",
           (unsigned)((hip >> 24) & 0xFFu), (unsigned)((hip >> 16) & 0xFFu),
           (unsigned)((hip >> 8) & 0xFFu), (unsigned)(hip & 0xFFu));
  Serial.print(F("[HubPick] "));
  Serial.println(dotted);
}

static int hub_pick_args_ok(const uint32_t *ips_nbo, const uint16_t *ports_hbo,
                            int n, uint32_t *out_ips_nbo,
                            uint16_t *out_ports_hbo, int out_max) {
  if (ips_nbo == NULL || ports_hbo == NULL || n <= 0) {
    return 0;
  }
  if (out_ips_nbo == NULL || out_ports_hbo == NULL || out_max <= 0) {
    return 0;
  }
  return 1;
}

static int hub_pick_copy(const uint32_t *ips_nbo, const uint16_t *ports_hbo,
                         const int *order, int ordered, uint32_t *out_ips_nbo,
                         uint16_t *out_ports_hbo, int out_max) {
  int i;
  int idx;
  int written = 0;
  for (i = 0; i < ordered && written < out_max; i++) {
    idx = order[i];
    out_ips_nbo[written] = ips_nbo[idx];
    out_ports_hbo[written] = ports_hbo[idx];
    hub_pick_log_ip(ips_nbo[idx]);
    written++;
  }
  return written;
}

// Returns how many ordered IPv4s were written, or -1.
extern "C" int rr_wifi_hub_choose(uint32_t sta_ip_nbo, uint32_t sta_nm_nbo,
                                  const uint32_t *ips_nbo,
                                  const uint16_t *ports_hbo, int n,
                                  uint32_t *out_ips_nbo,
                                  uint16_t *out_ports_hbo, int out_max) {
  uint32_t host_ips[10];
  int order[10];
  int count = n;
  int ordered;
  int i;

  if (!hub_pick_args_ok(ips_nbo, ports_hbo, n, out_ips_nbo, out_ports_hbo,
                        out_max)) {
    return -1;
  }
  if (count > 10) {
    count = 10;
  }
  for (i = 0; i < count; i++) {
    host_ips[i] = ntohl(ips_nbo[i]);
  }
  ordered = wifi_hub_order_indices(ntohl(sta_ip_nbo), ntohl(sta_nm_nbo),
                                   host_ips, count, order, 10);
  hub_pick_log_count(ordered);
  if (ordered <= 0) {
    return -1;
  }
  return hub_pick_copy(ips_nbo, ports_hbo, order, ordered, out_ips_nbo,
                       out_ports_hbo, out_max);
}

#endif
