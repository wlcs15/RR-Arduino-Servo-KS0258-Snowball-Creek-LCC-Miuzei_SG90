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

- Portable ladder + motion: `lib/rr_servo` (host tests pass)
- Mega serial bring-up compiles (`sketches/TurnoutBringup`)
- Mega sketch with Snowball Creek CAN pins compiles; LCC events not linked (`libLCC` is GPL-2.0)
- Adafruit PCA9685 + BusIO submodules
- Other CPUs and TFT: not started (branch names reserved)

## Mega bring-up

```bash
git submodule update --init --recursive
ln -s "$(pwd)/lib/rr_servo" "$HOME/Arduino/libraries/rr_servo"
# Arduino IDE: sketches/TurnoutBringup
# FQBN arduino:avr:mega:cpu=atmega2560
./tests/run_host_tests.sh
```

Serial 115200: `t` throw, `c` close, `s` status.

OpenLCB IDs: OwlThree **05.01.01.01.A5.*** — first servo node **05.01.01.01.A5.02** (`.A5.01` is the existing D1 R32 display node).

## CPU branches

`main` stays Mega + KS0258 + Snowball Creek. These branches exist and start at the same commit; firmware for those boards is not started.

| Branch | Hardware |
| --- | --- |
| `wemos-d1r32` | Wemos TTgo D1 R32 / ESPDuino-32, KS0258 or PCA9685 module, OpenMRNIDF |
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
| **DEBUG** | Firmware and a **host DEBUG CLI** (fake ADC, `t`/`c` commands, print limit/motion). `-DDEBUG`, extra serial/printf, no optimization. Separate from Unity. |
| **Unit Test** | **Unity** (ThrowTheSwitch), git submodule. Same tests on **host** and **on-target** (Mega first). |
| **Cyclomatic complexity** | **lizard**, report **per file / per module**. **Fail if any function in our code exceeds 10.** No fail (and no requirement) on `third_party/`, git submodules, or other code we did not author. |
| **Coding standard** | **Google C++ Style + selected CERT** via **Clang-Tidy** and **Cppcheck**. **OCLint on Linux only** — not on Windows 10 or 11. This target **fails on error-severity only**; warnings are reports. |

Unity submodule and these targets are **policy**; scripts are not in the tree yet.

## Design origin

Project thread: https://grok.com/c/69fe95c1-c8ad-4e4e-88aa-e3a2d443f480?rid=44e18d88-0d74-4ee7-b9e5-f73b8c16ec33

Servo interface (shared snapshot): https://grok.com/share/bGVnYWN5LWNvcHk_41f592a8-f434-4dfc-b46a-4e5414fb2e57

Notes: [docs/design-conversation.md](docs/design-conversation.md).
