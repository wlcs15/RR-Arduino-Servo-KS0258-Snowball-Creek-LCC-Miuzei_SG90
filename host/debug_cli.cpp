/* Host DEBUG CLI: fake ADC + t/c commands. C++11, no Arduino. */

#include "LimitLadder.h"
#include "TurnoutChannel.h"

#include <cstdio>
#include <cstring>

#ifdef DEBUG
#define RR_LOG(...) std::printf(__VA_ARGS__)
#else
#define RR_LOG(...)
#endif

static char cmd_key(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static bool apply_command(const char *line, int *raw, TurnoutChannel *ch) {
  const char key = cmd_key(line[0]);
  if (key == 'q') {
    return false;
  }
  if (key == 'a') {
    int v = 0;
    if (std::sscanf(line + 1, "%d", &v) == 1) {
      *raw = v;
    }
  } else if (key == 't') {
    ch->command(TURNOUT_CMD_THROWN, 0);
  } else if (key == 'c') {
    ch->command(TURNOUT_CMD_CLOSED, 0);
  }
  return true;
}

static void print_state(int raw, int avg, const TurnoutChannel &ch) {
  std::printf("adc=%d avg=%d limit=%s motion=%s us=%u drive=%d\n", raw, avg,
              limit_state_name(ch.last_limit()), ch.motion_name(),
              (unsigned)ch.pulse_us(), ch.drive_enabled() ? 1 : 0);
}

int main(void) {
  LimitLadderConfig cfg = limit_ladder_default_10bit();
  LimitLadderFilter filter(4);
  TurnoutChannel ch;
  ch.set_config(turnout_default_sg90());
  int raw = 1023;
  char line[64];

  RR_LOG("debug-cli RR_USE_KS0258 host simulator\n");
  std::printf("commands: a <adc> | t | c | s | q\n");

  while (std::fgets(line, (int)sizeof(line), stdin) != NULL) {
    if (!apply_command(line, &raw, &ch)) {
      break;
    }
    const int avg = filter.push(raw);
    ch.update(0, limit_ladder_decode(avg, cfg));
    print_state(raw, avg, ch);
  }
  return 0;
}
