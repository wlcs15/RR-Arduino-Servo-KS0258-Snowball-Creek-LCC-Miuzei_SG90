#ifndef RR_SERVO_OPENMRN_WIFI_BOOT_H
#define RR_SERVO_OPENMRN_WIFI_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
  RR_WIFI_BOOT_SPIFFS = 0,
  RR_WIFI_BOOT_CDI_XML = 1,
  RR_WIFI_BOOT_CONFIG_FILE = 2,
  RR_WIFI_BOOT_WIFI_MGR = 3,
  RR_WIFI_BOOT_STACK_BEGIN = 4,
  RR_WIFI_BOOT_STEP_COUNT = 5
};

/* Legal order is a prefix of SPIFFS, CDI XML, config file, WiFi manager,
 * stack begin. WiFi manager before config file is the A5.03 abort. */
static int rr_wifi_boot_order_ok(const unsigned char *seq, int n) {
  int i;
  int expect;

  if (seq == 0 || n < 0) {
    return 0;
  }
  if (n > RR_WIFI_BOOT_STEP_COUNT) {
    return 0;
  }
  expect = 0;
  for (i = 0; i < n; i++) {
    if (seq[i] != (unsigned char)expect) {
      return 0;
    }
    expect++;
  }
  return 1;
}

/* OpenMRN ConfigUpdateFlow::open_file HASSERTs unless fd can be opened. */
static int rr_wifi_config_fd_ready(int config_fd) { return config_fd >= 0; }

/* SNIP FILE_LITERAL_BYTE requires ACDI version byte 2. */
static int rr_snip_acdi_version_ok(unsigned char byte0) { return byte0 == 2; }

#ifdef __cplusplus
}
#endif

#endif
