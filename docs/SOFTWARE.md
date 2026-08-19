# Software

The firmware is split so the same turnout logic can run on more than one Arduino-class board.

| Path | Role |
| --- | --- |
| `lib/rr_servo` | Portable decode + motion state machine (no Arduino types except in `BoardPins.h`) |
| `sketches/TurnoutBringup` | Mega/Uno serial bring-up: KS0258 + analog ladder |
| `sketches/LccTurnoutNode` | Same motion path plus Snowball Creek CAN pins; LCC events not linked (GPL `libLCC` avoided) |
| `tests/` | Host tests (today: a small C++ driver; policy is Unity on host and on-target) |

## Libraries

Submodules (BSD):

```bash
git submodule update --init --recursive
```

- `third_party/Adafruit-PWM-Servo-Driver-Library`
- `third_party/Adafruit_BusIO`

Do not add Snowball Creek **libLCC** (GPL-2.0). LCC on D1 R32 / ARM is OpenMRNIDF / OpenMRNLite.

Install this repo’s library:

```bash
ln -s "$(pwd)/lib/rr_servo" "$HOME/Arduino/libraries/rr_servo"
```

## Board targets

| Board | I2C to KS0258 | Analog feedback channels |
| --- | --- | --- |
| Uno / Nano | A4/A5 | A0–A3 (4) |
| Mega 2560 | jumper A4→20, A5→21, use `Wire` | A0–A3 and A6–A15 (14) |
| ESP32 (later) | GPIO SDA/SCL | 12-bit ADC; use `limit_ladder_scaled(4095)` |

Snowball Creek `libLCC` is a C library with incoming-frame / write callbacks, which is why the LCC sketch keeps turnout logic out of the CAN glue.

## Bring-up

1. Wire channel 0 and the A0 ladder as in `docs/HARDWARE.md`.
2. External 5 V on KS0258 V+, common ground.
3. Flash `TurnoutBringup` to the Mega at 115200 baud.
4. Confirm idle status shows `limit=neither` with ADC near 1023.
5. Close switch A by hand: `limit=thrown`, ADC near 330.
6. Close switch B: `limit=closed`. Both: `limit=both`.
7. Send `t` / `c`. The servo should stop when the destination switch closes.

Tune `TurnoutConfig` pulse widths after the 3D mount is installed. Tune `LimitLadderConfig` if your measured ADC bands are not near the table in the hardware doc.

## LCC events (later)

OwlThree range **05.01.01.01.A5.*** — first servo node **05.01.01.01.A5.02**.

| Event low byte | Direction | Meaning |
| --- | --- | --- |
| 0x00 | consume | Throw |
| 0x01 | consume | Close |
| 0x10 | produce | Neither limit |
| 0x11 | produce | Thrown limit |
| 0x12 | produce | Closed limit |
| 0x13 | produce | Both limits (fault) |

LCC event I/O on Mega waits for a BSD-licensed OpenLCB stack. D1 R32 / ARM will use OpenMRNIDF / OpenMRNLite.

## Host tests (interim)

Until Unity is added as a submodule, the stand-in driver is C++11:

```bash
g++ -std=c++11 -I lib/rr_servo/src tests/test_rr_servo.cpp -o /tmp/rr_servo_tests
/tmp/rr_servo_tests
```

Policy (Google C++ / CERT, lizard 10, DEBUG CLI, Unity, OCLint Linux-only) is in the root README. Do not run lizard fail, OCLint, or style-fail on `third_party/` or other submodules.
