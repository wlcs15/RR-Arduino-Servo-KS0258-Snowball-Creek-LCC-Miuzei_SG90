#ifndef RR_SERVO_LIMIT_LADDER_H
#define RR_SERVO_LIMIT_LADDER_H

#include <stdint.h>

// Four-state decode for the 10k / 4.7k / 2.2k resistor ladder on one analog pin.
// Thresholds are for a 10-bit ADC with 5 V AREF (Arduino Mega / Uno default).

enum LimitState {
  LIMIT_NEITHER = 0,
  LIMIT_THROWN = 1,  // switch A only
  LIMIT_CLOSED = 2,  // switch B only
  LIMIT_BOTH = 3
};

struct LimitLadderConfig {
  int neitherMin;  // raw > this => neither
  int thrownMin;   // raw > this => thrown (A)
  int closedMin;   // raw > this => closed (B); else both
};

inline LimitLadderConfig limit_ladder_default_10bit() {
  LimitLadderConfig c;
  c.neitherMin = 900;
  c.thrownMin = 250;
  c.closedMin = 150;
  return c;
}

// Scale the 10-bit defaults to another full-scale (e.g. 4095 on ESP32 12-bit).
inline LimitLadderConfig limit_ladder_scaled(int fullScale) {
  LimitLadderConfig d = limit_ladder_default_10bit();
  LimitLadderConfig c;
  c.neitherMin = (int)((long)d.neitherMin * fullScale / 1023);
  c.thrownMin = (int)((long)d.thrownMin * fullScale / 1023);
  c.closedMin = (int)((long)d.closedMin * fullScale / 1023);
  return c;
}

inline LimitState limit_ladder_decode(int raw, const LimitLadderConfig &cfg) {
  if (raw > cfg.neitherMin) {
    return LIMIT_NEITHER;
  }
  if (raw > cfg.thrownMin) {
    return LIMIT_THROWN;
  }
  if (raw > cfg.closedMin) {
    return LIMIT_CLOSED;
  }
  return LIMIT_BOTH;
}

inline const char *limit_state_name(LimitState s) {
  switch (s) {
    case LIMIT_NEITHER:
      return "neither";
    case LIMIT_THROWN:
      return "thrown";
    case LIMIT_CLOSED:
      return "closed";
    case LIMIT_BOTH:
      return "both";
    default:
      return "?";
  }
}

// Running average to settle mechanical bounce and analog noise.
class LimitLadderFilter {
 public:
  explicit LimitLadderFilter(uint8_t window = 4)
      : window_(window), index_(0), count_(0), sum_(0) {
    if (window_ < 1) {
      window_ = 1;
    }
    if (window_ > 16) {
      window_ = 16;
    }
    for (uint8_t i = 0; i < 16; ++i) {
      samples_[i] = 0;
    }
  }

  int push(int raw) {
    if (count_ == window_) {
      sum_ -= samples_[index_];
    } else {
      ++count_;
    }
    samples_[index_] = raw;
    sum_ += raw;
    index_ = (uint8_t)((index_ + 1) % window_);
    return sum_ / count_;
  }

  void reset() {
    index_ = 0;
    count_ = 0;
    sum_ = 0;
  }

 private:
  uint8_t window_;
  uint8_t index_;
  uint8_t count_;
  int samples_[16];
  long sum_;
};

#endif
