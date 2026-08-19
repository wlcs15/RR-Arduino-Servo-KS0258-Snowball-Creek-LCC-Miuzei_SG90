# Proposed branch: `wemos-d1r32`

Not on `main`. Cut when ESP32 firmware starts.

- Wemos TTgo D1 R32 / ESPDuino-32
- Servos: KS0258 shield **or** PCA9685 module on I2C
- LCC: OpenMRNIDF (BSD-2-Clause), same idea as `Arduino_Wemos_TTgo_D1_R32_ESPDuino-32_Waveshare_4inch_OpenMRN_WiFi`
- Node ID **05.01.01.01.A5.02** or later (`.A5.01` is the existing Wi-Fi display node)

For the 4" TFT **instead of** the KS0258 shield, use proposed branch `wemos-d1r32-tft` (SPI DIP D11/D12/D13). Keep servos on a PCA9685 module if both glass and servos are required.
