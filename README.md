# RR-Arduino-Servo-KS0258-Snowball-Creek-LCC-Miuzei_SG90

Under-layout **Miuzei SG90** turnout servos: one PCA9685 channel per machine, two microswitches on **one analog pin** (resistor ladder).

**`main` is the trunk for this hardware combination:**

- Arduino **Mega 2560**
- Keyestudio **KS0258** (16-channel PCA9685 servo shield)
- **Snowball Creek** LCC shield

Do not put other CPUs or the Waveshare 4" TFT on `main`. Those are later branches (see below).

Application code is **BSD-2-Clause**. Third-party trees are git **submodules** and keep their own licenses ([docs/LICENSES.md](docs/LICENSES.md)).

## Hardware (trunk)

```
5 V -- 10 kΩ -- analog pin -- 4.7 kΩ -- switch A (Thrown) -- GND
                    |     `-- 2.2 kΩ -- switch B (Closed) -- GND
                  100 nF to GND
```

Servo PWM is KS0258, not a Mega timer pin. Servo power is KS0258 **V+**, never the Mega 5 V pin.

Mega I2C: jumper KS0258 **A4→20** and **A5→21**. Snowball Creek keeps D2 (IRQ), D7 (EEPROM CS), D8 (CAN CS), ICSP SPI.

Full map: [docs/HARDWARE.md](docs/HARDWARE.md).

## Status

- Portable ladder + motion: `lib/rr_servo` (host Unity 8/8, llvm-cov **96.83%** lines)
- Mega serial bring-up: `sketches/TurnoutBringup` (DEBUG-off 45–135–90 cycle)
- Mega **KS0258 ch0 + ch1** proven with JMRI (**v1.02**): `sketches/LccTurnoutNode`
  - Snowball Creek **Rev 4 MCP2518** (ACAN2517), not MCP2515
  - Node **05.01.01.01.A5.02** (OwlThree; `.A5.01` is the D1 R32 display). Next free **`.A5.03`**
  - Power-up: hold 90° 1 s, one 45→135→45→90 sweep, then hold 90° until LCC
  - JMRI panel: [docs/jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml](docs/jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml)
    - Left (TO3 / ch0): `…A5.02.00.00` throw / `…A5.02.00.01` close
    - Right (TO4 / ch1): `…A5.02.01.00` throw / `…A5.02.01.01` close
  - Throw/close 1000/2000 µs until pulses are trimmed; command holds (does not snap to 90°)
  - Flash/SRAM vs one-servo JMRI build: **+840 B flash, +31 B SRAM** (35906 / 2103)
  - Firmware that links **LibLCC** is GPL-2.0; application sources stay BSD-2-Clause
- Limit ladder still unwired. Do not mount the SG90 until pulses match the 3D stops.
- **This branch (`wemos-d1r32`)**: ESP32 D1 R32 from **v1.02**. Node **05.01.01.01.A5.03**. Flashed **RrServoUnity** (8/8 on-target, no PWM). Hardware now: D1 R32 + Waveshare RS485 CAN **B00XMERZA4**. OpenMRN not started. Servo shields not fitted; D9/D10/I2C left free.

## Electrical stacks (keep separate)

**Mega 2560 trunk (5 V):** Snowball Creek + KS0258 + 5 V analog ladder + SG90 on KS0258 **V+**. Safe.

**Wemos D1 R32 (3.3 V GPIO) + Waveshare RS485 CAN Shield (Amazon B00XMERZA4):** correct 3.3 V CAN board for this CPU. Transceiver-only **SN65HVD230** + **MAX3485** RS485. **3.3 V only** — not Snowball Creek, not MCP2515, not LibLCC. CAN TX/RX are **D14/D15** (D1 R32 **GPIO21/22**, the SDA/SCL pads). Ignore RS485 (D7 enable, D8/D2 UART). Ignore CANADUINO Nano adapters.

Servo shields are **not** on the stack yet. Keep **D9/D10** free for Adafruit **1438** hobby servos. Keep **A4/A5** and extra I2C free for a later PCA9685. Analog ladder stays **A1** (A0 is GPIO2 boot strap), 3.3 V pull-up. SG90 power still 5 V, not the ESP32 5 V pin.

**KS0258 3.3 V?** Keyestudio lists logic **3–5 V**, so the PCA9685 *can* run at 3.3 V. The **Arduino-footprint shield** still ties I2C to **A4/A5**. On D1 R32 those are GPIO36/39 (**input-only**), so a stacked KS0258 does **not** talk I2C without jumpers to GPIO21/22 — and those pins are already **Waveshare CAN**. Do not stack KS0258 + B00XMERZA4. A PCA9685 **module** on other GPIOs could work later.

**Adafruit 1438 3.3 V?** Yes (IOREF / 2024 boards use SDA/SCL only). Hobby servos on **D9/D10** do not fight CAN. 1438 **DC/stepper I2C (0x60)** *does* fight CAN on GPIO21/22 — servo headers only if CAN stays stacked.

Do not mix the 5 V Mega shield stack onto the ESP32.

## Mega bring-up

```bash
git submodule update --init --recursive
ln -s "$(pwd)/lib/rr_servo" "$HOME/Arduino/libraries/rr_servo"
# Arduino IDE: sketches/TurnoutBringup
# FQBN arduino:avr:mega:cpu=atmega2560
./scripts/run_tests.sh
./scripts/run_cppcheck.sh
# optional: ./scripts/run_lizard.sh  ./scripts/run_clang_tidy.sh
# coverage (Clang llvm-cov, not gcovr/Ceedling/MinGW):
python scripts/run_coverage.py
# Mega Unity (+ optional gcov link): python scripts/compile_mega_unity.py --coverage
```

Serial 115200: `t` throw, `c` close, `s` status.

OpenLCB / LCC (5 V Mega trunk): Snowball Creek + Library Manager **LibLCC + ACAN2517 + M95_EEPROM** (Rev 4 is MCP2518). Node **05.01.01.01.A5.02**. Next free OwlThree ID is **`.A5.03`**. Do not reuse `.A5.01` (D1 R32 display). Do not use the shield EEPROM ID (`02.02.02.00.01.4C`) or `03.00.AB.01.*`.

```bash
# Library Manager (once per PC): LibLCC, ACAN2517, ACAN2515, M95_EEPROM
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 \
  --library lib/rr_servo --library third_party/Adafruit-PWM-Servo-Driver-Library \
  --library third_party/Adafruit_BusIO sketches/LccTurnoutNode
# Linux Mega is VID 2341:0042 (often /dev/ttyACM0). Win11 was COM5.
# Leave the serial monitor closed during JMRI clicks (DTR resets the node).
# Optional later: -DLCC_ON (no UART)  -DOPTIMIZE_MEMORY (-Os, smaller CAN FIFOs)
```

## ESP32-only features (standing rule)

From 23-Aug-2026, **new features are `#if defined(ARDUINO_ARCH_ESP32)`** (or equivalent) so they do **not** land in Mega 2560 binaries. Mega flash/SRAM stay for the proven KS0258 + Snowball Creek node. Shared `lib/rr_servo` motion/ladder stays portable. OpenMRN, extra diagnostics, Wi-Fi, and extra ESP32 PWM paths stay behind that gate.

## Compile-time flags

Do **not** turn every flag on at once. DEBUG serial fights `LCC_ON`. `HACK` only applies to the DEBUG-off sweep.

| Flag | This branch | Purpose |
| --- | --- | --- |
| **DEBUG** / **RR_DEBUG** | **ON** (`#define RR_DEBUG` in `TurnoutBringup`) | Hold **90° / 1500 µs**, extra serial, no 45–135 cycle. Captured as `RR_BRINGUP_DEBUG`, then `DEBUG` is `#undef`’d so Arduino headers do not fight it. Host DEBUG CLI uses the same name. |
| **HACK** | **OFF** (commented) | DEBUG-off path only: print `sweep_us=` every 15 ms. UART flood. Enable with `-DHACK` while debugging the lerp. `// #define HACK` is left commented in the sketch. |
| **LCC_ON** | OFF | Mega `LccTurnoutNode`: silence UART. Later memory pass. Opposite of DEBUG. |
| **OPTIMIZE_MEMORY** | OFF | Mega: `-Os` and smaller CAN FIFOs. Later memory pass. |
| **RR_USE_KS0258** | **0** on D1 R32 (1 on Mega) | 1 = PCA9685 / KS0258. 0 = Arduino `Servo` / ESP32Servo on fallback PWM pins. |
| **RR_CAN_CHIP** | 2518 (Mega only) | `2518` → ACAN2517 / MCP2518 (Snowball Creek Rev 4). Else ACAN2515. ESP32 uses TWAI + OpenMRN later. |
| **LIBLCC_EVENT_LIST_STATIC_SIZE** | 16 (Mega) | LibLCC static consumed-event table. Not used on ESP32. |
| **RR_GCOV** | OFF | Mega Unity gcov link. Host coverage is `RR_ENABLE_COVERAGE` + llvm-cov. |
| **RR_ENABLE_COVERAGE** | OFF unless `run_coverage.py` | Host Clang llvm-cov of `lib/rr_servo`. |
| **RR_LIMIT_PIN_0** | **A1** on ESP32 (A0 on Mega) | Analog ladder. D1 R32 **A0 is GPIO2 (boot strap)**. |
| **RR_FALLBACK_PWM_0** | **25** (D3/GPIO25) on ESP32; D44 on Mega | ESP32 LEDC with no 1438/KS0258. Next: GPIO17, GPIO16. Not D9/D10 (1438), not D2 (Waveshare RS485 RX). Mega fallback stays D44–D46 (`#else`). |
| **RR_TURNOUT_COUNT** | 1 on ESP32 first bring-up; 16 with KS0258 | Channel count. |
| **RR_PCA9685_ADDR** | 0x40 | I2C address if a PCA9685 (KS0258 / 1411 / 815) is used. 1438 motors are 0x60 and are not this path. |
| **RR_OWLTHREE_NODE_ID** | **05.01.01.01.A5.03** on ESP32 | Mega stays **`.A5.02`**. Display is **`.A5.01`**. |

ESP32 Arduino 3.x has no core `Servo.h`; use Library Manager **ESP32Servo**. FQBN: `esp32:esp32:d1_uno32`.

## Quality baseline (`wemos-d1r32`, 23-Aug-2026)

Host Unity **8/8**. On-target Unity **8/8** on D1 R32 (`sketches/RrServoUnity`, no PWM). Same eight tests as Mega.

lizard (CCN fail at 10; `third_party/` not scanned):

| Module | NLOC | Funcs | Avg CCN | Max CCN | CCN>10 |
| --- | ---: | ---: | ---: | ---: | ---: |
| host | 51 | 4 | 3.5 | 6 | 0 |
| lib/rr_servo | 282 | 30 | 2.1 | 5 | 0 |
| sketches/LccTurnoutNode | 423 | 24 | 3.4 | 8 | 0 |
| sketches/RrServoUnity | 31 | 5 | 2.0 | 6 | 0 |
| sketches/TurnoutBringup | 302 | 19 | 3.7 | 9 | 0 |
| tests | 112 | 12 | 1.6 | 4 | 0 |
| **TOTAL (our code)** | **1201** | **94** | **2.8** | **9** | **0** |

Host llvm-cov of `lib/rr_servo` (same suite Mega used): **lines 96.83%**, regions 97.14%, branches 93.94%, functions 100%.

On-target coverage **percent** is not available on either CPU:

| Target | What happens |
| --- | --- |
| Mega | `--coverage` links avr `libgcov.a`, but `__gcov_dump` is an empty stub; no `.gcda`, no % |
| ESP32 D1 R32 | `--coverage` **does not link** (`__gcov_init` / `__gcov_dump` / `__gcov_merge_add` undefined). No on-target % |

Compare ESP32 to Mega using the **host** llvm-cov of the same tests, not a target gcov number.

## CPU branches

`main` stays Mega + KS0258 + Snowball Creek. **`wemos-d1r32` is this line** (fast-forwarded from v1.02). Do not merge ESP32 pin maps back onto `main`.

| Branch | Hardware |
| --- | --- |
| `wemos-d1r32` | Wemos TTgo D1 R32 + Waveshare RS485 CAN B00XMERZA4; analog A1; D9/D10 reserved for 1438; OpenMRN later |
| `uno-r3` | Arduino Uno R3 + KS0258 (few analog channels; Snowball Creek still fits) |
| `arm-arduino` | TBD Arduino-footprint ARM + KS0258 or PCA9685 module |

### Waveshare 4" TFT (no KS0258 shield)

The Mega trunk already stacks **two** Uno-footprint shields (Snowball Creek + KS0258). A Waveshare / Coowell 4" ILI9486 Arduino TFT is a **third** shield and fights those headers (SPI D11–D13 on D1 R32; Mega SPI is ICSP vs D11–D13).

Treat display as **instead of** the KS0258 **shield**, not stacked with it:

1. Keep servos on a **PCA9685 module** on flying I2C (SDA/SCL), not the KS0258 shield, so the Arduino headers stay free for the TFT.
2. Name those branches with a `-tft` suffix, **forked from the matching CPU branch after that CPU line diverges** (not created yet):

| Proposed branch | Meaning |
| --- | --- |
| `wemos-d1r32-tft` | D1 R32 + Waveshare 4" TFT; PCA9685 **module** if servos are still required |
| `uno-r3-tft` | Uno R3 + 4" TFT; same module rule |
| `arm-arduino-tft` | ARM Arduino footprint + 4" TFT |

Do **not** create `mega2560-tft` that also keeps KS0258. If Mega ever needs glass, drop the KS0258 shield first.

Shared turnout logic stays in `lib/rr_servo` on `main` and is merged into CPU branches. Display UI stays on the `-tft` branch.

More detail: [docs/BRANCHES.md](docs/BRANCHES.md).

## Language, KS0258 gate, and host builds

**Our** new code is **C11** and **C++11** only (no later language features). Third-party libraries and submodules may use newer standards; we wrap or isolate them.

KS0258 / PCA9685 paths are wrapped in `#if RR_USE_KS0258` … `#endif`. The trunk default is **enabled**:

```c
#ifndef RR_USE_KS0258
#define RR_USE_KS0258 1
#endif
```

Turn it off with `-DRR_USE_KS0258=0` (or a board config). Then drive uses the Arduino **Servo** library on reserved PWM pins (not a DAC). See **Reserved pins** below — do **not** use D9/D10; the Waveshare 4" TFT takes those for backlight and LCD_CS.

Host programs (not Arduino sketches) must build on **Ubuntu x86** and **Windows 11 with LLVM Clang**, `-std=c11` / `-std=c++11`. Host scope: `lib/rr_servo` plus tests and a DEBUG CLI. Sketches stay Arduino-only.

## Reserved pins (all Arduino-footprint boards)

Chosen so Snowball Creek, a future Waveshare 4" ILI9486 TFT (SKU 13587), and KS0258-off PWM do not fight. The 4" shield uses **D3–D13** (TP_IRQ, TP_CS, SD_CS, TP_BUSY, LCD_DC, LCD_RST, LCD_BL=D9, LCD_CS=D10, MOSI/MISO/SCK).

| Use | Pin | Notes |
| --- | --- | --- |
| Limit ladder 0 | **A0** | TFT does not use analog. Pull-up to the MCU I/O voltage (5 V on Mega/Uno, **3.3 V on ESP32**). |
| Limit ladder 1–3 | A1–A3 | |
| Limit ladder 4+ (Mega) | A6–A15 | Skip A4/A5 if they are I2C. |
| I2C SDA/SCL | Mega **20/21** (jumper A4/A5); Uno **A4/A5**; D1 R32 **SDA/SCL (GPIO21/22)** | KS0258 shield or PCA9685 **module**. |
| KS0258 on (`RR_USE_KS0258=1`) | PCA9685 ch 0, 1, … | Default on `main`. |
| KS0258 off, Mega, no TFT | **D44**, then D45, D46 | Mega Timer5 PWM. **Not** on the Uno shield header, so a later TFT does not steal them. |
| KS0258 off + TFT (any Uno-footprint CPU) | **PCA9685 module on I2C** | D3–D13 are gone. Uno and D1 R32 have **no** spare header PWM. |
| Do not use for servos or analog | D2, D7, D8, ICSP | Snowball Creek CAN/EEPROM/SPI. |
| Do not use for servos | D3–D13 | Waveshare 4" TFT (and Uno SPI). |

Snowball Creek **cannot** stack with the Waveshare 4" TFT: both need D7 and D8 (and D3). TFT nodes use Wi-Fi/OpenMRN, not the LCC shield.

Host programs (not Arduino sketches) must build on **Ubuntu x86** and **Windows 11 with LLVM Clang**, `-std=c11` / `-std=c++11`. Host scope: `lib/rr_servo` plus tests and a DEBUG CLI. Sketches stay Arduino-only.

## Build targets (to implement)

| Target | Meaning |
| --- | --- |
| **DEBUG** | Firmware and a **host DEBUG CLI** (fake ADC, `t`/`c` commands, print limit/motion). `-DDEBUG`, extra serial/printf, no optimization. Separate from Unity. First Mega bring-up holds every KS0258 channel at **90 deg (1500 us)** on a 0–180 SG90 map (1000/1500/2000 us). **Do not flash that onto a servo whose horn is already at a mechanical stop** (see below). |
| **Unit Test** | **Unity** (ThrowTheSwitch), git submodule. Same tests on **host** and **on-target** (Mega first). |
| **Cyclomatic complexity** | **lizard**, report **per module** (`lib/rr_servo`, each sketch, `tests`, `host`). Arduino `.ino` included. **Fail if any function in our code exceeds 10** (McCabe is per-function; module table is NLOC / function count / avg / max CCN). No fail on `third_party/` or other submodules. Windows: `python scripts/run_lizard.py`. |
| **Coding standard** | **Google C++ Style + selected CERT** via **Clang-Tidy** and **Cppcheck**. **OCLint on Linux only** — not on Windows 10 or 11. This target **fails on error-severity only**; warnings are reports. |

Host CMake (C11/C++11, Clang on Ubuntu and Windows 11): `python scripts/build_host.py` or `scripts/build_host.sh`. Unity tests: `scripts/run_tests.sh`. DEBUG CLI: `build/host/rr_servo_debug_cli`. Coverage is Clang source-based (`-fprofile-instr-generate -fcoverage-mapping`) plus **llvm-cov** — not gcovr, Ceedling, or MinGW. Usual command: `python scripts/run_coverage.py`. Same tree: `cmake --build build/host-coverage --target coverage`. Report is `lib/rr_servo` only; HTML is `build/host-coverage/coverage/index.html`. Mega bring-up drives **16** KS0258 channels (`n`/`p` select, `t`/`c` throw/close). Channels 14–15 have no analog pin (A4/A5 reserved for I2C). This branch’s ESP32 bring-up is `sketches/TurnoutBringup` with DEBUG on and `RR_USE_KS0258=0`.

## Design origin

Project thread: https://grok.com/c/69fe95c1-c8ad-4e4e-88aa-e3a2d443f480?rid=44e18d88-0d74-4ee7-b9e5-f73b8c16ec33

Servo interface (shared snapshot): https://grok.com/share/bGVnYWN5LWNvcHk_41f592a8-f434-4dfc-b46a-4e5414fb2e57

Notes: [docs/design-conversation.md](docs/design-conversation.md).
