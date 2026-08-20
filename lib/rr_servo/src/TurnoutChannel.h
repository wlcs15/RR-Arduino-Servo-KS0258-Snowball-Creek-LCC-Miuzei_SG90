#ifndef RR_SERVO_TURNOUT_CHANNEL_H
#define RR_SERVO_TURNOUT_CHANNEL_H

#include "LimitLadder.h"
#include <stdint.h>

enum TurnoutCommand { TURNOUT_CMD_NONE = 0, TURNOUT_CMD_THROWN, TURNOUT_CMD_CLOSED };

enum TurnoutMotion {
  TURNOUT_IDLE = 0,
  TURNOUT_MOVING_THROWN,
  TURNOUT_MOVING_CLOSED,
  TURNOUT_FAULT
};

enum TurnoutFault {
  TURNOUT_FAULT_NONE = 0,
  TURNOUT_FAULT_BOTH_LIMITS,
  TURNOUT_FAULT_TIMEOUT,
  TURNOUT_FAULT_WRONG_LIMIT
};

// Miuzei SG90: 0–180 deg (not 360). 1000 us = 0, 1500 us = 90, 2000 us = 180.
enum {
  RR_SG90_US_0 = 1000,
  RR_SG90_US_90 = 1500,
  RR_SG90_US_180 = 2000
};

struct TurnoutConfig {
  uint16_t thrownPulseUs;
  uint16_t closedPulseUs;
  uint32_t moveTimeoutMs;
  uint32_t holdAfterLimitMs;  // keep PWM after limit, then release
  bool releasePwmWhenIdle;    // stop buzzing the SG90 after arrival
};

inline TurnoutConfig turnout_default_sg90() {
  TurnoutConfig c;
  c.thrownPulseUs = (uint16_t)RR_SG90_US_180;
  c.closedPulseUs = (uint16_t)RR_SG90_US_0;
  c.moveTimeoutMs = 4000;
  c.holdAfterLimitMs = 200;
  c.releasePwmWhenIdle = true;
  return c;
}

class TurnoutChannel {
 public:
  TurnoutChannel()
      : cfg_(turnout_default_sg90()),
        motion_(TURNOUT_IDLE),
        lastCommand_(TURNOUT_CMD_NONE),
        lastLimit_(LIMIT_NEITHER),
        fault_(TURNOUT_FAULT_NONE),
        moveStartedMs_(0),
        arrivedMs_(0),
        arrived_(false),
        drive_(false),
        pulseUs_((uint16_t)RR_SG90_US_90) {}

  void set_config(const TurnoutConfig &cfg) { cfg_ = cfg; }
  const TurnoutConfig &config() const { return cfg_; }

  void command(TurnoutCommand cmd, uint32_t nowMs) {
    if (cmd == TURNOUT_CMD_NONE) {
      return;
    }
    lastCommand_ = cmd;
    fault_ = TURNOUT_FAULT_NONE;
    arrived_ = false;
    arrivedMs_ = 0;
    moveStartedMs_ = nowMs;
    drive_ = true;
    if (cmd == TURNOUT_CMD_THROWN) {
      motion_ = TURNOUT_MOVING_THROWN;
      pulseUs_ = cfg_.thrownPulseUs;
    } else {
      motion_ = TURNOUT_MOVING_CLOSED;
      pulseUs_ = cfg_.closedPulseUs;
    }
  }

  void update(uint32_t nowMs, LimitState limit) {
    lastLimit_ = limit;
    if (limit == LIMIT_BOTH) {
      enter_fault(TURNOUT_FAULT_BOTH_LIMITS);
      return;
    }
    update_while_moving(nowMs, limit);
    maybe_release_pwm(nowMs);
  }

  TurnoutMotion motion() const { return motion_; }
  TurnoutFault fault() const { return fault_; }
  TurnoutCommand last_command() const { return lastCommand_; }
  LimitState last_limit() const { return lastLimit_; }
  bool drive_enabled() const { return drive_; }
  uint16_t pulse_us() const { return pulseUs_; }
  bool arrived() const { return arrived_; }

  const char *motion_name() const {
    switch (motion_) {
      case TURNOUT_IDLE:
        return "idle";
      case TURNOUT_MOVING_THROWN:
        return "moving-thrown";
      case TURNOUT_MOVING_CLOSED:
        return "moving-closed";
      case TURNOUT_FAULT:
        return "fault";
      default:
        return "?";
    }
  }

  const char *fault_name() const {
    switch (fault_) {
      case TURNOUT_FAULT_NONE:
        return "none";
      case TURNOUT_FAULT_BOTH_LIMITS:
        return "both-limits";
      case TURNOUT_FAULT_TIMEOUT:
        return "timeout";
      case TURNOUT_FAULT_WRONG_LIMIT:
        return "wrong-limit";
      default:
        return "?";
    }
  }

 private:
  void update_while_moving(uint32_t nowMs, LimitState limit) {
    LimitState want;
    if (motion_ == TURNOUT_MOVING_THROWN) {
      want = LIMIT_THROWN;
    } else if (motion_ == TURNOUT_MOVING_CLOSED) {
      want = LIMIT_CLOSED;
    } else {
      return;
    }
    if (limit == want) {
      mark_arrived(nowMs);
    } else if ((nowMs - moveStartedMs_) >= cfg_.moveTimeoutMs) {
      enter_fault(TURNOUT_FAULT_TIMEOUT);
    }
  }

  void maybe_release_pwm(uint32_t nowMs) {
    if (motion_ != TURNOUT_IDLE) {
      return;
    }
    if (!arrived_ || !cfg_.releasePwmWhenIdle) {
      return;
    }
    if ((nowMs - arrivedMs_) >= cfg_.holdAfterLimitMs) {
      drive_ = false;
    }
  }

  void mark_arrived(uint32_t nowMs) {
    motion_ = TURNOUT_IDLE;
    arrived_ = true;
    arrivedMs_ = nowMs;
    drive_ = true;
  }

  void enter_fault(TurnoutFault f) {
    fault_ = f;
    motion_ = TURNOUT_FAULT;
    drive_ = false;
    arrived_ = false;
  }

  TurnoutConfig cfg_;
  TurnoutMotion motion_;
  TurnoutCommand lastCommand_;
  LimitState lastLimit_;
  TurnoutFault fault_;
  uint32_t moveStartedMs_;
  uint32_t arrivedMs_;
  bool arrived_;
  bool drive_;
  uint16_t pulseUs_;
};

#endif
