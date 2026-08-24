#ifndef RR_SERVO_WIFI_HUB_PICK_H
#define RR_SERVO_WIFI_HUB_PICK_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Rank IPv4 hub addresses advertised by a dual-homed JMRI LCC hub.
// Host-order uint32. Lower rank is tried first. Rank 1000 is skipped.
// Does not change the Linux host: firmware tries reachable IPv4s.

#ifndef WIFI_HUB_RANK_SKIP
#define WIFI_HUB_RANK_SKIP 1000
#endif

inline uint32_t wifi_hub_ipv4_octets(unsigned a, unsigned b, unsigned c,
                                     unsigned d) {
  return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
         (uint32_t)d;
}

inline int wifi_hub_ipv4_usable(uint32_t ip) {
  const unsigned a = (unsigned)((ip >> 24) & 0xFFu);
  const unsigned b = (unsigned)((ip >> 16) & 0xFFu);
  if (ip == 0u) {
    return 0;
  }
  if (a == 127u) {
    return 0;
  }
  if (a == 169u && b == 254u) {
    return 0;
  }
  if (a >= 224u) {
    return 0;
  }
  return 1;
}

inline int wifi_hub_ipv4_parse(const char *s, uint32_t *out) {
  unsigned a = 0;
  unsigned b = 0;
  unsigned c = 0;
  unsigned d = 0;
  if (s == NULL || s[0] == '\0' || out == NULL) {
    return 0;
  }
  if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
    return 0;
  }
  if (a > 255u || b > 255u || c > 255u || d > 255u) {
    return 0;
  }
  *out = wifi_hub_ipv4_octets(a, b, c, d);
  return wifi_hub_ipv4_usable(*out);
}

inline int wifi_hub_ipv4_rank(uint32_t sta_ip, uint32_t sta_mask,
                              uint32_t cand) {
  if (!wifi_hub_ipv4_usable(cand) || cand == sta_ip) {
    return WIFI_HUB_RANK_SKIP;
  }
  if (sta_mask != 0u && (cand & sta_mask) == (sta_ip & sta_mask)) {
    return 0;
  }
  return 1;
}

// Fill out[] with candidate indices in try order (same-subnet first, mDNS
// order preserved within a rank). Returns count written, 0 if none usable.
// Dual-homed JMRI on one /24 yields two rank-0 IPv4s; the caller must try
// each until TCP works. Do not change the Linux host.
inline int wifi_hub_order_indices(uint32_t sta_ip, uint32_t sta_mask,
                                  const uint32_t *cands, int n, int *out,
                                  int out_max) {
  int written = 0;
  int pass;
  int i;
  if (cands == NULL || n <= 0 || out == NULL || out_max <= 0) {
    return 0;
  }
  for (pass = 0; pass <= 1; pass++) {
    for (i = 0; i < n && written < out_max; i++) {
      if (wifi_hub_ipv4_rank(sta_ip, sta_mask, cands[i]) == pass) {
        out[written] = i;
        written++;
      }
    }
  }
  return written;
}

#endif
