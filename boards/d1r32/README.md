# Branch: `wemos-d1r32`

Active. Fast-forwarded from `main` v1.02. Do not land this pin map on `main`.

- Wemos TTgo D1 R32 / ESPDuino-32 (FQBN `esp32:esp32:d1_uno32`)
- No KS0258 shield, no Snowball Creek
- Fitted now: **Waveshare RS485 CAN Shield B00XMERZA4** (3.3 V, SN65HVD230, TWAI on GPIO21/22)
- Servo shields **not** fitted. **D9/D10** reserved for Adafruit **1438** hobby servos. Do not stack **KS0258** with this CAN shield (A4/A5 input-only; SDA/SCL is CAN)
- Analog ladder: **A1** (3.3 V pull-up). A0 is GPIO2 (boot strap)
- Node **05.01.01.01.A5.03** (`.A5.01` display, `.A5.02` Mega)
- On-target tests: `RrServoUnity` (no PWM). Bring-up sketch still has DEBUG on / HACK off
- LCC later: OpenMRNIDF. Not LibLCC

For the 4" TFT **instead of** a servo shield, use proposed branch `wemos-d1r32-tft` (SPI DIP D11/D12/D13). D9/D10 are TFT backlight/CS, so 1438 servos cannot share that stack.
