#ifndef RR_SERVO_WIFI_STA_BIND_H
#define RR_SERVO_WIFI_STA_BIND_H

#include <stddef.h>
#include <string.h>

// Models Esp32WiFiManager: SSID and PSK are copied at construct time.
// Filling the live buffers later does not update an existing bind.
// Empty PSK + WPA2 AP is ESP-IDF reason 210
// (WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY).

enum WifiStaBindStatus {
  WIFI_STA_BIND_OK = 0,
  WIFI_STA_BIND_NO_SSID = 1,
  WIFI_STA_BIND_NO_PSK = 2
};

struct WifiStaBind {
  char ssid[33];
  char psk[65];
};

inline void wifi_sta_bind_clear(WifiStaBind *bind) {
  if (bind == NULL) {
    return;
  }
  memset(bind, 0, sizeof(*bind));
}

inline void wifi_sta_bind_copy(WifiStaBind *bind, const char *ssid,
                               const char *psk) {
  wifi_sta_bind_clear(bind);
  if (bind == NULL) {
    return;
  }
  if (ssid != NULL) {
    strncpy(bind->ssid, ssid, sizeof(bind->ssid) - 1);
  }
  if (psk != NULL) {
    strncpy(bind->psk, psk, sizeof(bind->psk) - 1);
  }
}

inline WifiStaBindStatus wifi_sta_bind_status(const WifiStaBind *bind) {
  if (bind == NULL || bind->ssid[0] == '\0') {
    return WIFI_STA_BIND_NO_SSID;
  }
  if (bind->psk[0] == '\0') {
    return WIFI_STA_BIND_NO_PSK;
  }
  return WIFI_STA_BIND_OK;
}

inline int wifi_sta_bind_wpa2_ready(const WifiStaBind *bind) {
  return wifi_sta_bind_status(bind) == WIFI_STA_BIND_OK ? 1 : 0;
}

#endif
