#ifndef RR_SERVO_CDI_WELLFORMED_H
#define RR_SERVO_CDI_WELLFORMED_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LCC Pro Configure needs a complete CDI XML (space 0xFF). JMRI JDOM fails
 * with "must start and end within the same entity" if the document is cut. */

static int rr_cdi_has(const char *xml, unsigned len, const char *needle) {
  unsigned n;
  if (xml == 0 || needle == 0) {
    return 0;
  }
  n = (unsigned)strlen(needle);
  if (n == 0 || n > len) {
    return 0;
  }
  {
    unsigned i;
    for (i = 0; i + n <= len; i++) {
      if (memcmp(xml + i, needle, n) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

/* 1 = Pro can parse Configure. 0 = truncated or missing CDI. */
static int rr_cdi_configure_ready(const char *xml, unsigned len) {
  if (xml == 0 || len < 16) {
    return 0;
  }
  if (!rr_cdi_has(xml, len, "<cdi")) {
    return 0;
  }
  if (!rr_cdi_has(xml, len, "</cdi>")) {
    return 0;
  }
  if (!rr_cdi_has(xml, len, "<manufacturer>")) {
    return 0;
  }
  return 1;
}

/* Copy CDI in JMRI MemoryConfig chunk sizes (typically 64). */
static unsigned rr_cdi_jmri_read(const char *src, unsigned src_len, unsigned off,
                                 char *dst, unsigned chunk) {
  unsigned room;
  if (src == 0 || dst == 0 || off >= src_len) {
    return 0;
  }
  room = src_len - off;
  if (chunk > room) {
    chunk = room;
  }
  memcpy(dst, src + off, chunk);
  return chunk;
}

#ifdef __cplusplus
}
#endif

#endif
