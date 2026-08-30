#ifndef RR_SERVO_RAM_OPENLCB_CFG_H
#define RR_SERVO_RAM_OPENLCB_CFG_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RR_RAMCFG_BYTES = 1024 };

#define RR_RAMCFG_PATH "/ramcfg/openlcb"

typedef struct {
  unsigned char buf[RR_RAMCFG_BYTES];
  long off;
} RrRamCfg;

/* Fill 0xFF and ACDI version byte 2 (SNIP FILE_LITERAL_BYTE). */
static void rr_ramcfg_init(RrRamCfg *c) {
  if (c == 0) {
    return;
  }
  memset(c->buf, 0xFF, RR_RAMCFG_BYTES);
  c->buf[0] = 2;
  c->off = 0;
}

static int rr_ramcfg_lseek(RrRamCfg *c, long offset, int whence) {
  long next;

  if (c == 0) {
    return -1;
  }
  if (whence == 0) {
    next = offset;
  } else if (whence == 1) {
    next = c->off + offset;
  } else if (whence == 2) {
    next = (long)RR_RAMCFG_BYTES + offset;
  } else {
    return -1;
  }
  if (next < 0 || next > (long)RR_RAMCFG_BYTES) {
    return -1;
  }
  c->off = next;
  return (int)c->off;
}

static int rr_ramcfg_read(RrRamCfg *c, void *dst, unsigned size) {
  unsigned room;

  if (c == 0 || dst == 0) {
    return -1;
  }
  if (c->off < 0 || c->off >= (long)RR_RAMCFG_BYTES) {
    return 0;
  }
  room = (unsigned)((long)RR_RAMCFG_BYTES - c->off);
  if (size > room) {
    size = room;
  }
  memcpy(dst, c->buf + (unsigned)c->off, size);
  c->off += (long)size;
  return (int)size;
}

static int rr_ramcfg_write(RrRamCfg *c, const void *src, unsigned size) {
  unsigned room;

  if (c == 0 || src == 0) {
    return -1;
  }
  if (c->off < 0 || c->off >= (long)RR_RAMCFG_BYTES) {
    return 0;
  }
  room = (unsigned)((long)RR_RAMCFG_BYTES - c->off);
  if (size > room) {
    size = room;
  }
  memcpy(c->buf + (unsigned)c->off, src, size);
  c->off += (long)size;
  return (int)size;
}

static int rr_ramcfg_fstat_size(const RrRamCfg *c) {
  if (c == 0) {
    return -1;
  }
  return RR_RAMCFG_BYTES;
}

#ifdef __cplusplus
}
#endif

#endif
