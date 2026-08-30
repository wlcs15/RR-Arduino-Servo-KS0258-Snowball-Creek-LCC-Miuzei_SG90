// ESP32-only: OpenMRNLite LCC over Wi-Fi (RR_WIFI_LCC).
// Node 05.01.01.01.A5.03. Mega LibLCC is not used here.
//
// Wi-Fi PSK is never in git and never typed into Grok. Same wrap as
// Arduino_Wemos_TTgo_D1_R32_ESPDuino-32_Waveshare_4inch_OpenMRN_WiFi:
//   python scripts/build_lcc_wifi.py --flash --port COM7
//   python scripts/collect_hw_ids.py --port COM7
//   python scripts/provision_wifi_build.py   // YOUR TTY only
//   python scripts/build_lcc_wifi.py --flash --port COM7
//
// SSID SRIF2333 is public. JMRI OpenLCB hub GridConnect TCP port 12021.
// Throw/close events (ch0): ...A5.03.00.00 / ...A5.03.00.01

#ifndef RR_WIFI_LCC
#define RR_WIFI_LCC 1
#endif

#if !defined(ARDUINO_ARCH_ESP32)
#error "LccWifiTurnoutNode is ESP32-only (RR_WIFI_LCC)."
#endif

#include <Arduino.h>
#include <SPIFFS.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <OpenMRNLite.h>
#include "openlcb/CallbackEventHandler.hxx"

#include <BoardPins.h>
#include "WifiHubPick.h"
#include "WifiStaBind.h"
#include "RamOpenlcbCfg.h"
#include "config.h"
#include "wifi_cred.h"

namespace openlcb {
const SimpleNodeStaticValues SNIP_STATIC_DATA = {
    4, "OwlThree", "RR Servo D1 R32 (WiFi)", "D1R32",
    RR_GIT_VERSION_LITERAL};
}  // namespace openlcb
#include <esp_vfs.h>
#include <sys/stat.h>

static constexpr uint64_t NODE_ID = (uint64_t)RR_OWLTHREE_NODE_ID;
static const uint64_t kEventBase = 0x05010101A5030000ULL;

static char wifi_ssid[33] = "SRIF2333";
static char wifi_psk[65];

OpenMRN openmrn(NODE_ID);
string dummystring("abcdef");
static constexpr openlcb::ConfigDef cfg(0);
// Esp32WiFiManager copies SSID/PSK into std::string in its constructor.
// A file-scope instance would copy the empty wifi_psk[] before setup()
// unwraps NVS, then join WPA2 with no password (ESP-IDF reason 210).
static Esp32WiFiManager *wifi_mgr = NULL;

static uint64_t evt_throw(unsigned ch) {
  return kEventBase + ((uint64_t)ch << 8);
}

static uint64_t evt_close(unsigned ch) {
  return kEventBase + ((uint64_t)ch << 8) + 1ULL;
}

static void on_lcc_event(const openlcb::EventRegistryEntry &entry,
                         openlcb::EventReport *report,
                         BarrierNotifiable *done) {
  (void)report;
  (void)done;
  if (entry.event == evt_throw(0)) {
    Serial.println(F("lcc throw ch0"));
  } else if (entry.event == evt_close(0)) {
    Serial.println(F("lcc close ch0"));
  } else if (entry.event == evt_throw(1)) {
    Serial.println(F("lcc throw ch1"));
  } else if (entry.event == evt_close(1)) {
    Serial.println(F("lcc close ch1"));
  }
}

openlcb::CallbackEventHandler lcc_events(openmrn.stack()->node(), on_lcc_event);

class FactoryResetHelper : public DefaultConfigUpdateListener {
 public:
  UpdateAction apply_configuration(int fd, bool initial_load,
                                   BarrierNotifiable *done) OVERRIDE {
    AutoNotify n(done);
    if (initial_load) {
      cfg.seg().wifi().sleep().write(fd, 0);
      cfg.seg().wifi().uplink().last_address().ip_address().write(fd, "");
      cfg.seg().wifi().uplink().reconnect().write(fd, 0);
    }
    return UPDATED;
  }

  void factory_reset(int fd) override {
    cfg.userinfo().name().write(fd, openlcb::SNIP_STATIC_DATA.model_name);
    cfg.userinfo().description().write(fd, "OwlThree turnout node Wi-Fi");
  }
};

static FactoryResetHelper *s_reset_helper;

namespace openlcb {
const char CDI_FILENAME[] = "/ramcfg/cdi.xml";
const char CDI_DATA[] = "";
/* Arduino-ESP32 3.x SPIFFS POSIX read() returns EIO; OpenMRN HASSERTs.
 * RAM VFS (same as the D1 R32 display node) is the config file. */
const char *const CONFIG_FILENAME = RR_RAMCFG_PATH;
const size_t CONFIG_FILE_SIZE = cfg.seg().size() + cfg.seg().offset();
/* OpenMRN create_config HASSERTs this equals CONFIG_FILENAME. */
const char *const SNIP_DYNAMIC_FILENAME = CONFIG_FILENAME;
}  // namespace openlcb

static_assert(openlcb::CONFIG_FILE_SIZE <= RR_RAMCFG_BYTES,
              "OpenLCB config larger than RAM VFS");

static RrRamCfg s_ramcfg;
static unsigned char s_cdi[8192];
static unsigned s_cdi_len;
static long s_ram_off[4];
static uint8_t s_ram_used[4];
static uint8_t s_ram_kind[4];

static int ram_slot(int fd) {
  int i = fd - 3;
  if (i < 0 || i > 3) {
    return -1;
  }
  return i;
}

static int ram_open(const char *path, int flags, int mode) {
  int i;
  const uint8_t kind = (path && strstr(path, "cdi")) ? 1 : 0;
  (void)flags;
  (void)mode;
  for (i = 0; i < 4; i++) {
    if (!s_ram_used[i]) {
      s_ram_used[i] = 1;
      s_ram_kind[i] = kind;
      s_ram_off[i] = 0;
      return i + 3;
    }
  }
  s_ram_off[0] = 0;
  s_ram_kind[0] = kind;
  return 3;
}

static int ram_close(int fd) {
  int i = ram_slot(fd);
  if (i >= 0) {
    s_ram_used[i] = 0;
  }
  return 0;
}

static unsigned ram_cap(int i) {
  return s_ram_kind[i] ? (unsigned)sizeof(s_cdi) : (unsigned)RR_RAMCFG_BYTES;
}

static unsigned char *ram_base(int i) {
  return s_ram_kind[i] ? s_cdi : s_ramcfg.buf;
}

static ssize_t ram_write(int fd, const void *data, size_t size) {
  int i = ram_slot(fd);
  unsigned cap;
  unsigned room;
  if (i < 0 || data == 0) {
    return -1;
  }
  cap = ram_cap(i);
  if (s_ram_off[i] < 0 || (unsigned)s_ram_off[i] >= cap) {
    return 0;
  }
  room = cap - (unsigned)s_ram_off[i];
  if (size > room) {
    size = room;
  }
  memcpy(ram_base(i) + (unsigned)s_ram_off[i], data, size);
  s_ram_off[i] += (long)size;
  if (s_ram_kind[i] && (unsigned)s_ram_off[i] > s_cdi_len) {
    s_cdi_len = (unsigned)s_ram_off[i];
  }
  return (ssize_t)size;
}

static ssize_t ram_read(int fd, void *dst, size_t size) {
  int i = ram_slot(fd);
  unsigned cap;
  unsigned room;
  if (i < 0 || dst == 0) {
    return -1;
  }
  cap = ram_cap(i);
  if (s_ram_kind[i] && s_cdi_len < cap) {
    cap = s_cdi_len;
  }
  if (s_ram_off[i] < 0 || (unsigned)s_ram_off[i] >= cap) {
    return 0;
  }
  room = cap - (unsigned)s_ram_off[i];
  if (size > room) {
    size = room;
  }
  memcpy(dst, ram_base(i) + (unsigned)s_ram_off[i], size);
  s_ram_off[i] += (long)size;
  return (ssize_t)size;
}

static off_t ram_lseek(int fd, off_t offset, int whence) {
  int i = ram_slot(fd);
  unsigned cap;
  long next;
  if (i < 0) {
    return -1;
  }
  cap = ram_cap(i);
  if (whence == SEEK_SET) {
    next = (long)offset;
  } else if (whence == SEEK_CUR) {
    next = s_ram_off[i] + (long)offset;
  } else if (whence == SEEK_END) {
    next = (long)cap + (long)offset;
  } else {
    return -1;
  }
  if (next < 0 || next > (long)cap) {
    return -1;
  }
  s_ram_off[i] = next;
  return (off_t)next;
}

static int ram_fstat(int fd, struct stat *st) {
  int i = ram_slot(fd);
  if (!st || i < 0) {
    return -1;
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFREG | 0666;
  st->st_size = s_ram_kind[i] ? (off_t)s_cdi_len : (off_t)RR_RAMCFG_BYTES;
  return 0;
}

static void mount_ramcfg(void) {
  static int mounted;
  esp_vfs_t vfs;
  if (mounted) {
    return;
  }
  rr_ramcfg_init(&s_ramcfg);
  memset(&vfs, 0, sizeof(vfs));
  vfs.flags = ESP_VFS_FLAG_DEFAULT;
  vfs.open = ram_open;
  vfs.close = ram_close;
  vfs.write = ram_write;
  vfs.read = ram_read;
  vfs.lseek = ram_lseek;
  vfs.fstat = ram_fstat;
  if (esp_vfs_register("/ramcfg", &vfs, NULL) == ESP_OK) {
    mounted = 1;
  } else {
    Serial.println(F("[boot] ramcfg VFS failed"));
  }
}

static void print_debug_ids(void) {
  uint8_t mac[6];
  uint8_t uid[8];
  bool uid_ok = false;
  char line[40];
  wifi_hw_ids_read(mac, uid, &uid_ok);
  const uint64_t node = wifi_node_id();

  Serial.println(F("==== DEBUG IDs (not the WiFi PSK) ===="));
  snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  Serial.print(F("MAC Address: "));
  Serial.println(line);
  snprintf(line, sizeof(line), "%02X.%02X.%02X.%02X.%02X.%02X",
           (unsigned)((node >> 40) & 0xFFu), (unsigned)((node >> 32) & 0xFFu),
           (unsigned)((node >> 24) & 0xFFu), (unsigned)((node >> 16) & 0xFFu),
           (unsigned)((node >> 8) & 0xFFu), (unsigned)(node & 0xFFu));
  Serial.print(F("OpenLCB Node ID: "));
  Serial.println(line);
  if (uid_ok) {
    snprintf(line, sizeof(line), "%02X%02X%02X%02X%02X%02X%02X%02X", uid[0],
             uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
  } else {
    snprintf(line, sizeof(line), "UNAVAILABLE");
  }
  Serial.print(F("SPI flash unique ID: "));
  Serial.println(line);
}

static void load_wifi_creds(void) {
  const esp_err_t err =
      wifi_cred_load(wifi_ssid, sizeof(wifi_ssid), wifi_psk, sizeof(wifi_psk));
  if (err == ESP_OK) {
    Serial.println(F("WIFI PASSWORD: SET (NOT SHOWN)"));
    Serial.print(F("SSID "));
    Serial.println(wifi_ssid);
    Serial.print(F("WIFI PSK chars: "));
    Serial.println((unsigned)strlen(wifi_psk));
  } else if (err == ESP_ERR_NOT_FOUND) {
    Serial.println(F("WIFI PASSWORD: NOT SET"));
  } else {
    Serial.println(F("WIFI PASSWORD: UNWRAP FAIL"));
  }
}

static void start_wifi_manager(void) {
  WifiStaBind bind;
  wifi_sta_bind_copy(&bind, wifi_ssid, wifi_psk);
  memset(wifi_psk, 0, sizeof(wifi_psk));
  if (!wifi_sta_bind_wpa2_ready(&bind)) {
    Serial.println(F("WiFi manager not started (no PSK)"));
    wifi_sta_bind_clear(&bind);
    return;
  }
  wifi_mgr = new Esp32WiFiManager(bind.ssid, bind.psk, openmrn.stack(),
                                  cfg.seg().wifi());
  wifi_sta_bind_clear(&bind);
}

static bool mount_spiffs(void) {
  if (SPIFFS.begin()) {
    return true;
  }
  Serial.println(F("SPIFFS format and remount"));
  return SPIFFS.begin(true);
}

static void start_openmrn(void) {
  mount_ramcfg();
  Serial.println(F("[boot] CDI xml"));
  openmrn.create_config_descriptor_xml(cfg, openlcb::CDI_FILENAME);
  Serial.print(F("[boot] CONFIG_FILENAME="));
  Serial.println(openlcb::CONFIG_FILENAME ? openlcb::CONFIG_FILENAME : "(null)");
  s_reset_helper = new FactoryResetHelper();
  /* Esp32WiFiManager::factory_reset must run during create_config or hub
   * port/host stay 0xFF (65535 / garbage hostname) and the uplink panics. */
  start_wifi_manager();
  Serial.println(F("[boot] create_config_file"));
  openmrn.stack()->create_config_file_if_needed(
      cfg.seg().internal_config(), openlcb::CANONICAL_VERSION,
      openlcb::CONFIG_FILE_SIZE);
  s_ramcfg.buf[0] = 2;
}

static void finish_openmrn(void) {
  Serial.println(F("[boot] openmrn.begin"));
  openmrn.begin();
  lcc_events.add_entry(evt_throw(0), openlcb::CallbackEventHandler::IS_CONSUMER);
  lcc_events.add_entry(evt_close(0), openlcb::CallbackEventHandler::IS_CONSUMER);
  lcc_events.add_entry(evt_throw(1), openlcb::CallbackEventHandler::IS_CONSUMER);
  lcc_events.add_entry(evt_close(1), openlcb::CallbackEventHandler::IS_CONSUMER);
  openmrn.start_executor_thread();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
  }
  Serial.println(F("LccWifiTurnoutNode RR_WIFI_LCC OpenMRNLite GridConnect"));
  Serial.print(F("firmware "));
  Serial.println(F(RR_GIT_VERSION_LITERAL));
  Serial.print(F("SNIP "));
  Serial.print(openlcb::SNIP_STATIC_DATA.manufacturer_name);
  Serial.print(F(" / "));
  Serial.print(openlcb::SNIP_STATIC_DATA.model_name);
  Serial.print(F(" / "));
  Serial.println(openlcb::SNIP_STATIC_DATA.software_version);
  nvs_flash_init();
  print_debug_ids();
  load_wifi_creds();
  if (!mount_spiffs()) {
    Serial.println(F("SPIFFS failed"));
    return;
  }
  start_openmrn();
  finish_openmrn();
}

void loop() {
  openmrn.loop();
}
