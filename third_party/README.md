# Third-party submodules

```bash
git submodule update --init --recursive
```

| Path | Upstream | License |
| --- | --- | --- |
| `Adafruit_BusIO` | https://github.com/adafruit/Adafruit_BusIO | BSD |
| `Adafruit-PWM-Servo-Driver-Library` | https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library | BSD |
| `Unity` | https://github.com/ThrowTheSwitch/Unity | MIT |

Still to add when the CPU branch needs them:

- OpenMRNIDF 5.1.x for `cpu/d1r32` (same vendor branch pattern as `Arduino_Wemos_TTgo_D1_R32_ESPDuino-32_Waveshare_4inch_OpenMRN_WiFi`)
- Waveshare / ImpulseAdventure ILI9486 if the 4" shield is enabled
- Not libLCC (GPL-2.0) — see [docs/LICENSES.md](../docs/LICENSES.md)
