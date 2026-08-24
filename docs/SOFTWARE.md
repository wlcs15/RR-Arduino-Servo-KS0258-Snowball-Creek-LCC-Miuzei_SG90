# Software

The firmware is split so the same turnout logic can run on more than one Arduino-class board.

| Path | Role |
| --- | --- |
| `lib/rr_servo` | Portable decode + motion state machine (no Arduino types except in `BoardPins.h`) |
| `sketches/TurnoutBringup` | Mega/Uno serial bring-up: KS0258 + analog ladder |
| `sketches/LccTurnoutNode` | Same motion path plus Snowball Creek CAN. Small LCC node via Library Manager **LibLCC + ACAN2517 + M95_EEPROM** (Rev 4 MCP2518). Linking LibLCC makes that firmware image GPL-2.0; do not copy it into `lib/`. |
| `docs/jmri/` | Canonical JMRI Layout Editor panels (copy into `~/.jmri/…`; do not commit the live profile) |
| `tests/` | Host tests (today: a small C++ driver; policy is Unity on host and on-target) |

## Libraries

Submodules (BSD):

```bash
git submodule update --init --recursive
```

- `third_party/Adafruit-PWM-Servo-Driver-Library`
- `third_party/Adafruit_BusIO`

5 V Arduino LCC uses Snowball Creek’s stack (**LibLCC + ACAN2517 + M95_EEPROM**, Library Manager; Rev 4 is MCP2518). LibLCC is GPL-2.0; that Mega/Uno **binary** is GPL. Do not vendor-copy it into `lib/`. 3.3 V nodes (ESP32) use **OpenMRNLite** (Arduino-ESP32, `RR_WIFI_LCC`) or OpenMRNIDF later. Not Snowball Creek.

ESP32 Wi-Fi PSK uses the same host-encrypt wrap as `Arduino_Wemos_TTgo_D1_R32_ESPDuino-32_Waveshare_4inch_OpenMRN_WiFi`: Grok never sees the password; `scripts/provision_wifi_build.py` is TTY-only.

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

## LCC events

OwlThree range **05.01.01.01.A5.00–FF**. Node **05.01.01.01.A5.02** (`.A5.01` is the D1 R32 display). Next free **`.A5.03`**. Factory EEPROM ID is printed, not used. Channel index is byte 6 of the event.

| Event | Direction | Meaning |
| --- | --- | --- |
| `05.01.01.01.A5.02.00.00` | consume | ch0 throw |
| `05.01.01.01.A5.02.00.01` | consume | ch0 close |
| `05.01.01.01.A5.02.01.00` | consume | ch1 throw |
| `05.01.01.01.A5.02.01.01` | consume | ch1 close |
| `05.01.01.01.A5.02.00.10`–`.13` | produce | ch0 limits (neither/thrown/closed/both) |

Panel: [docs/jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml](jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml). Snowball Creek Rev 4 uses **MCP2518** (ACAN2517). D1 R32 / ARM 3.3 V nodes use OpenMRNIDF / OpenMRNLite.

## Host tests

Unity is a git submodule (`third_party/Unity`). Host binaries are CMake + **Clang** (C11/C++11) on Ubuntu x86 and Windows 11:

```bash
python scripts/build_host.py
python scripts/run_coverage.py
```

`run_coverage.py` configures `build/host-coverage` with `-DRR_ENABLE_COVERAGE=ON`, builds, runs Unity, then **llvm-cov** (not gcovr, Ceedling, MinGW, or AVR-GCC). After that tree exists you can also run `cmake --build build/host-coverage --target coverage`. The report covers `lib/rr_servo` only.

On-target Unity (same eight tests as the host, no servo PWM) is `sketches/RrServoUnity`.

```bash
python scripts/compile_mega_unity.py
python scripts/compile_mega_unity.py --coverage
python scripts/compile_mega_unity.py --coverage --upload --port COM5
```

`--coverage` links Arduino avr-gcc **libgcov.a** (`lib/gcc/avr/7.3.0/avr6/`) and a tiny `__gcov_exit` shim (`sketches/RrServoUnity/gcov_exit.c`) because that archive has `__gcov_dump` but not `__gcov_exit`. Only `test_rr_servo.cpp` is instrumented so SRAM stays under 8 KB (instrumenting the Arduino core overflows Mega RAM).

Do **not** add a tiny EEPROM/SD filesystem for dump: `avr-nm` on this `libgcov.a` shows `__gcov_dump` has **no** `fopen`/`fwrite` (empty AVR stub). SdFat/LittleFS/TEFS need SPI/SD and fight Snowball Creek; EepromFS/OSFS cannot hook a dump that never opens files. Line coverage percent is the host Clang llvm-cov report of the same tests.

Policy (Google C++ / CERT, lizard 10, DEBUG CLI, Unity, OCLint Linux-only) is in the root README. Lizard prints a per-module table (`lib/rr_servo`, each sketch, `tests`, `host`) and still fails if any **function** CCN exceeds 10. Do not run lizard fail, OCLint, or style-fail on `third_party/` or other submodules.
