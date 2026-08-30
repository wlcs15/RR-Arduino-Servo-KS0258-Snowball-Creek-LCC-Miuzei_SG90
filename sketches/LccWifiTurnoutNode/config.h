#ifndef RR_LCC_WIFI_CONFIG_H
#define RR_LCC_WIFI_CONFIG_H

#include "openlcb/ConfigRepresentation.hxx"
#include "openlcb/MemoryConfig.hxx"
#include "freertos_drivers/esp32/Esp32WiFiConfiguration.hxx"
#if defined(__has_include)
#if __has_include("git_version.inc")
#include "git_version.inc"
#endif
#endif
#include "GitVersion.h"

namespace openlcb
{

/* Defined in LccWifiTurnoutNode.ino so it overrides OpenMRNLite's weak
 * "Undefined model" SNIP_STATIC_DATA. */
extern const SimpleNodeStaticValues SNIP_STATIC_DATA;

static constexpr uint16_t CANONICAL_VERSION = 0x2001;

CDI_GROUP(WifiNodeSegment, Segment(MemoryConfigDefs::SPACE_CONFIG), Offset(128));
CDI_GROUP_ENTRY(internal_config, InternalConfigData);
CDI_GROUP_ENTRY(wifi, WiFiConfiguration, Name("WiFi Configuration"));
CDI_GROUP_END();

CDI_GROUP(ConfigDef, MainCdi());
CDI_GROUP_ENTRY(ident, Identification);
CDI_GROUP_ENTRY(acdi, Acdi);
CDI_GROUP_ENTRY(userinfo, UserInfoSegment, Name("User Info"));
CDI_GROUP_ENTRY(seg, WifiNodeSegment, Name("Settings"));
CDI_GROUP_END();

}  // namespace openlcb

#endif
