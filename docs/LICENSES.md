# Licenses

This repository’s own code is **BSD-2-Clause** (see root `LICENSE`), matching OpenMRN / OpenMRNLite and the Adafruit PCA9685 driver.

Submodules keep the license of their upstream. A submodule does **not** re-license that code as BSD-2-Clause.

| Tree | License | Role |
| --- | --- | --- |
| This repo (sketches, `lib/rr_servo`, docs) | BSD-2-Clause | Application |
| Adafruit PWM Servo Driver / BusIO | BSD | KS0258 PCA9685 |
| OpenMRN / OpenMRNLite / OpenMRNIDF | BSD-2-Clause | LCC on ESP32 / ARM |
| Waveshare / community ILI9486 + XPT2046 | typically MIT | optional 4" TFT |
| Snowball Creek **libLCC** (`rm5248/liblcc-arduino`) | **GPL-2.0** | Official Arduino library in the shield user guide |

## Do not pull libLCC if you want a BSD-only product

The Grok thread recommended BSD-2-Clause specifically so LCC could ship with OpenMRN on the larger-memory boards (Mega 2560, D1 R32, ARM) without copyleft.

Snowball Creek’s Library Manager stack (`libLCC` + ACAN2515 + M95_EEPROM) is convenient on the Mega, but **libLCC is GPL-2.0**. Linking it into the Mega firmware would put that binary under GPL-2.0. That is the opposite of the license choice in the conversation.

Plan:

- **D1 R32 and ARM:** OpenMRNIDF / OpenMRNLite as submodules (BSD-2-Clause).
- **Mega 2560:** servo + analog limits with Adafruit PWM (BSD). Add LCC on Mega only if you accept GPL-2.0 for that binary, or if a BSD OpenLCB AVR port is chosen later.
- Never copy GPL sources into `lib/` as if they were this repo’s BSD code.
