# Branches

## Trunk

**`main`** is the only line for:

Arduino Mega 2560 + Keyestudio KS0258 + Snowball Creek LCC shield + Miuzei SG90 + analog dual-limit ladder.

Do not land Wemos, Uno-only, ARM, or Waveshare TFT work on `main`.

## CPU branches

`wemos-d1r32` is cut and fast-forwarded from `main` **v1.02**. `uno-r3` and `arm-arduino` still sit at the old snapshot.

| Branch | Board | Servo drive | LCC |
| --- | --- | --- | --- |
| `wemos-d1r32` | Wemos TTgo D1 R32 / ESPDuino-32 | Adafruit 1438 D10/D9 (ESP32Servo). Not KS0258. | OpenMRNIDF later; Waveshare RS485 CAN (TWAI). Node **`.A5.03`** |
| `uno-r3` | Arduino Uno R3 | KS0258 (A4/A5 I2C; A0–A3 analog only) | Snowball Creek fits; same libLCC-vs-OpenMRN issue as Mega |
| `arm-arduino` | TBD Arduino-footprint ARM | KS0258 or PCA9685 module | OpenMRN / OpenMRNLite |

## Display without the KS0258 shield

Uno-footprint **Waveshare 4" TFT** and the **KS0258 shield** both want the Arduino female headers. Snowball Creek already occupies the Mega’s shield stack on `main`. Do not stack all three.

Rule: a `-tft` branch **removes the KS0258 shield**. If that node still drives servos, use a PCA9685 **module** on I2C flying leads (or the Mega’s 20/21 with the TFT on the headers).

| Branch | Base | Shields |
| --- | --- | --- |
| `wemos-d1r32-tft` | `wemos-d1r32` | Waveshare 4" (SPI DIP D11/D12/D13 on D1 R32); optional PCA9685 module |
| `uno-r3-tft` | `uno-r3` | 4" TFT; optional PCA9685 module |
| `arm-arduino-tft` | `arm-arduino` | 4" TFT; optional PCA9685 module |

Shared motion/ladder code stays on `main` (`lib/rr_servo`) and is merged forward. TFT UI, ILI9486 drivers, and pin maps stay on the `-tft` branch.

The Waveshare 4" shield uses D3–D13 (including D7/D8/D9/D10). That overlaps Snowball Creek. A `-tft` node is not an LCC-shield stack. Servos on those branches use a PCA9685 module on I2C, not header PWM.

OwlThree OpenLCB range: **05.01.01.01.A5.00–FF**. Display: `.A5.01`. Mega servo node: **`.A5.02`**. This D1 R32 servo node: **`.A5.03`**.
