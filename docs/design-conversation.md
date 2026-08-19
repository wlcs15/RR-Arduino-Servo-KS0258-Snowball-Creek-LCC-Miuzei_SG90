# Design conversation (Grok)

Full thread: https://grok.com/c/69fe95c1-c8ad-4e4e-88aa-e3a2d443f480?rid=44e18d88-0d74-4ee7-b9e5-f73b8c16ec33

Created 2026-08-19. Title at last turn: *No exact commercial dual-limit-switch divider products found*.

Earlier public share of the same circuit (2026-08-15):
https://grok.com/share/bGVnYWN5LWNvcHk_41f592a8-f434-4dfc-b46a-4e5414fb2e57

## 1. Circuit (turns 1–4)

Mega 2560 + Snowball Creek LCC + KS0258. One servo drive path. One analog pin reports two QXAD0141 / ASIN B07YQQ3R7W limit switches: A, B, neither, or both.

Adopted ladder:

- 10 kΩ pull-up from 5 V to analog
- 4.7 kΩ in series with switch A (Thrown) to GND
- 2.2 kΩ in series with switch B (Closed) to GND
- Optional 100 nF analog to GND
- Servo on KS0258 PCA9685, independent V+

Approximate 10-bit ADC: neither ~1023, A ~330, B ~185, both ~125.

No commercial Futaba/JR harness was found that already includes that divider. DIY 3-pin servo + 3-pin switch plug.

## 2. Repository naming (turns 5–8)

You asked for a name in the `wlcs15` style and a **generic** repo with **CPU branches**:

1. Arduino Mega 2560
2. Wemos ESP32 TTgo D1 R32 ESPDuino-32
3. Generic ARM Arduino-footprint board (later)
4. Optional Arduino-footprint 4" TFT touch shield

Grok suggested `RR-Servo-Turnout-LCC`. The repo you created is the longer hardware-specific name, which matches your other board-heavy names (`Arduino_Wemos_TTgo_D1_R32_…`, `RR-Railcom-…`).

## 3. License and submodules (turns 9–10)

Pick **BSD-2-Clause** so application code matches OpenMRN / OpenMRNLite. Pull third-party trees in as **git submodules**; each keeps its own license. Avoid GPL as the project license.

See [LICENSES.md](LICENSES.md) for the Snowball Creek `libLCC` (GPL-2.0) vs OpenMRN (BSD-2-Clause) split.
